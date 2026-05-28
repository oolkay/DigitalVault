/*
 * DigitalVault — Vault Manager
 *
 * LUKS (dm-crypt) + ext4 tabanlı şifreli disk yöneticisi.
 * Biyometrik doğrulama ile LUKS anahtarını türetir,
 * şifreli bölümü açar/kapatır ve ext4 mount işlemlerini yapar.
 *
 * Kullanım:
 *   vault_manager setup  <device> [finger_id]
 *   vault_manager open   <device> <mountpoint> [finger_id]
 *   vault_manager close  <mountpoint>
 *   vault_manager status
 *
 * NOT: Bu binary root yetkisi gerektirir (cryptsetup + mount).
 *      GUI tarafından "pkexec vault_manager ..." şeklinde çağrılır.
 *      pkexec sayesinde grafik şifre diyaloğu gösterilir,
 *      terminal gerekmez.
 *
 *      Mount sonrası dosyalara hem dosya yöneticisinden (Nautilus vb.)
 *      hem de terminalden erişilebilir.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#include "hal.h"

/* ─── Constants ──────────────────────────────────────────────────────────── */

#define MAPPER_NAME     "digitalvault"
#define MAPPER_PATH     "/dev/mapper/" MAPPER_NAME
#define HAL_KEY_LEN     32      /* HAL'dan gelen master key boyutu */
#define HAL_SOCKET      "simulator:/tmp/biometric_vault.sock"
#define SETUP_TMPDIR    "/tmp/_vault_setup"

/* close / forceclose exit codes */
#define EXIT_CLOSE_MOUNT_BUSY   2

/* ─── Utility: Secure Memory Wipe ────────────────────────────────────────── */

/**
 * Bellekteki hassas veriyi güvenli şekilde sil.
 * Derleyici optimizasyonlarının bu işlemi atlamasını engeller.
 */
static void secure_wipe(void *ptr, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) *p++ = 0;
}

/* ─── Utility: Run External Command ──────────────────────────────────────── */

/**
 * Harici bir komutu fork+exec ile çalıştır.
 * @return 0 başarılı, -1 hata
 */
static int run_cmd(const char *argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("[vault_manager] fork");
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        execvp(argv[0], (char *const *)argv);
        perror("[vault_manager] execvp");
        _exit(127);
    }

    /* Parent: wait for child */
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

/**
 * cryptsetup'a key'i stdin üzerinden pipe'la.
 * Key asla komut satırında veya dosyada görünmez.
 *
 * @param argv      cryptsetup argümanları (NULL-terminated)
 * @param key       Pipe'lanacak key
 * @param key_len   Key uzunluğu
 * @return 0 başarılı, -1 hata
 */
static int cryptsetup_with_key(const char *argv[],
                               const uint8_t *key, size_t key_len)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("[vault_manager] pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("[vault_manager] fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: stdin'i pipe'ın read ucuna bağla */
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        execvp("cryptsetup", (char *const *)argv);
        perror("[vault_manager] execvp cryptsetup");
        _exit(127);
    }

    /* Parent: key'i pipe'a yaz ve kapat */
    close(pipefd[0]);

    ssize_t written = write(pipefd[1], key, key_len);
    close(pipefd[1]);

    if (written != (ssize_t)key_len) {
        fprintf(stderr, "[vault_manager] Failed to write key to pipe\n");
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }

    fprintf(stderr, "[vault_manager] cryptsetup exited with code %d\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    return -1;
}

/* ─── HAL Integration ────────────────────────────────────────────────────── */

/**
 * Biyometrik doğrulama yaparak şifreleme anahtarını al.
 *
 * @param finger_id  Doğrulanacak parmak izi ID'si
 * @param key_out    32 byte key çıktısı
 * @return 0 başarılı, -1 hata
 */
static int read_key_from_stdin(uint8_t *key_out)
{
    size_t got = 0;
    while (got < HAL_KEY_LEN) {
        ssize_t n = read(STDIN_FILENO, key_out + got, HAL_KEY_LEN - got);
        if (n < 0) {
            perror("[vault_manager] read stdin key");
            return -1;
        }
        if (n == 0) {
            fprintf(stderr, "[vault_manager] EOF reading key from stdin (%zu/%d bytes)\n",
                    got, HAL_KEY_LEN);
            return -1;
        }
        got += (size_t)n;
    }
    return 0;
}

static int get_biometric_key(int finger_id, uint8_t *key_out)
{
    printf("[vault_manager] Connecting to biometric sensor...\n");

    if (hal_init(HAL_SOCKET) != HAL_OK) {
        fprintf(stderr, "[vault_manager] HAL connection failed!\n");
        fprintf(stderr, "[vault_manager] Is the simulator running?\n");
        return -1;
    }

    int matched_id = 0;
    int ret = hal_verify_and_get_key(&matched_id, key_out);
    if (ret != HAL_OK) {
        fprintf(stderr, "[vault_manager] Fingerprint verification failed: %s\n",
                hal_strerror(ret));
        hal_cleanup();
        return -1;
    }

    if (finger_id > 0 && matched_id != finger_id) {
        fprintf(stderr, "[vault_manager] Matched finger %d but expected %d\n",
                matched_id, finger_id);
        secure_wipe(key_out, HAL_KEY_LEN);
        hal_cleanup();
        return -1;
    }

    printf("[vault_manager] Fingerprint #%d verified successfully!\n", matched_id);
    hal_cleanup();
    return 0;
}

/* ─── Device Resolution ─────────────────────────────────────────────────── */

/**
 * Cihazın LUKS olup olmadığını kontrol et.
 * @return 0 evet, -1 hayır
 */
static int is_luks_device(const char *device)
{
    const char *args[] = {"cryptsetup", "isLuks", device, NULL};
    return run_cmd(args);
}

/**
 * Verilen cihaz yolunu "akıllı" şekilde çözümle.
 * Eğer kullanıcı /dev/sdb verdiyse ve o LUKS değilse, /dev/sdb1'i dene.
 */
static void resolve_vault_device(const char *input, char *output, size_t maxlen)
{
    if (maxlen == 0)
        return;
    output[maxlen - 1] = '\0';

    if (is_luks_device(input) == 0) {
        strncpy(output, input, maxlen - 1);
        return;
    }

    const int is_loop = (strncmp(input, "/dev/loop", 9) == 0);
    char alt[256];

    if (is_loop) {
        snprintf(alt, sizeof(alt), "%sp1", input);
        if (is_luks_device(alt) == 0) {
            printf("[vault_manager] Found LUKS on loop partition: %s\n", alt);
            strncpy(output, alt, maxlen - 1);
            return;
        }
    } else {
        snprintf(alt, sizeof(alt), "%s1", input);
        if (is_luks_device(alt) == 0) {
            printf("[vault_manager] Found LUKS on partition: %s\n", alt);
            strncpy(output, alt, maxlen - 1);
            return;
        }
    }

    strncpy(output, input, maxlen - 1);
}

typedef enum {
    VAULT_STATE_CLEAN = 0,
    VAULT_STATE_MAPPER_ONLY,
    VAULT_STATE_MOUNTED_OURS,
    VAULT_STATE_MOUNTED_FOREIGN
} VaultRuntimeState;

static int mapper_exists(const char *mapper_path)
{
    return mapper_path && access(mapper_path, F_OK) == 0;
}

static int is_mountpoint_path(const char *path)
{
    if (!path || !*path)
        return 0;
    const char *args[] = {"mountpoint", "-q", path, NULL};
    return run_cmd(args) == 0;
}

static int read_mount_source(const char *mount_point, char *source, size_t source_len)
{
    if (!mount_point || !source || source_len == 0)
        return 0;

    source[0] = '\0';
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "findmnt -no SOURCE --target %s 2>/dev/null", mount_point);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return 0;

    if (!fgets(source, (int)source_len, fp)) {
        pclose(fp);
        return 0;
    }
    pclose(fp);
    source[strcspn(source, "\n")] = '\0';
    return source[0] != '\0';
}

static int mount_source_matches_mapper(const char *mount_point, const char *mapper_path)
{
    char source[256];
    if (!read_mount_source(mount_point, source, sizeof(source)))
        return 0;

    if (strcmp(source, mapper_path) == 0)
        return 1;

    /* findmnt may report /dev/dm-N; accept our mapper name */
    if (strstr(source, MAPPER_NAME) != NULL)
        return 1;

    return 0;
}

static int is_our_vault_mount(const char *mount_point, const char *mapper_path)
{
    return is_mountpoint_path(mount_point)
        && mount_source_matches_mapper(mount_point, mapper_path);
}

static VaultRuntimeState probe_vault_state(const char *mount_point, const char *mapper_path)
{
    const int mapper_open = mapper_exists(mapper_path);
    const int mounted = is_mountpoint_path(mount_point);

    if (!mapper_open && !mounted)
        return VAULT_STATE_CLEAN;
    if (mapper_open && !mounted)
        return VAULT_STATE_MAPPER_ONLY;
    if (mounted && mount_source_matches_mapper(mount_point, mapper_path))
        return VAULT_STATE_MOUNTED_OURS;
    if (mounted)
        return VAULT_STATE_MOUNTED_FOREIGN;
    return VAULT_STATE_CLEAN;
}

static int close_mapper_path(const char *mapper_path)
{
    if (!mapper_path || !mapper_exists(mapper_path))
        return 0;
    char mapper_name[128];
    const char *slash = strrchr(mapper_path, '/');
    const char *name = slash ? slash + 1 : mapper_path;
    snprintf(mapper_name, sizeof(mapper_name), "%s", name);
    const char *close_args[] = {"cryptsetup", "close", mapper_name, NULL};
    return run_cmd(close_args);
}

static int close_mapper(void)
{
    return close_mapper_path(MAPPER_PATH);
}

static void report_mount_busy(const char *mount_point, const char *mapper_path)
{
    fprintf(stderr, "[vault_manager] Mount point is busy: %s\n", mount_point);
    if (!is_our_vault_mount(mount_point, mapper_path)) {
        fprintf(stderr, "[vault_manager] Path is not our vault mount; skipping fuser report.\n");
        return;
    }
    const char *fuser_args[] = {"fuser", "-M", "-vm", mount_point, NULL};
    run_cmd(fuser_args);
}

static int kill_pids_from_fuser(const char *mount_point, int use_sigkill)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "fuser -M -m %s 2>/dev/null", mount_point);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return -1;

    char buf[4096] = {0};
    const size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    const int popen_status = pclose(fp);
    if (n == 0 || popen_status != 0)
        return 0;

    const pid_t self = getpid();
    const pid_t parent = getppid();
    const int sig = use_sigkill ? SIGKILL : SIGTERM;

    char *save = NULL;
    for (char *tok = strtok_r(buf, " \t\n", &save); tok; tok = strtok_r(NULL, " \t\n", &save)) {
        if (!isdigit((unsigned char)tok[0]))
            continue;
        const pid_t pid = (pid_t)atoi(tok);
        if (pid <= 1 || pid == self || pid == parent)
            continue;
        kill(pid, sig);
    }

    return 0;
}

static int kill_mount_holders(const char *mount_point, const char *mapper_path)
{
    if (!is_our_vault_mount(mount_point, mapper_path)) {
        fprintf(stderr,
                "[vault_manager] Refusing to kill processes: %s is not a verified vault mount.\n",
                mount_point);
        return -1;
    }

    fprintf(stderr, "[vault_manager] Stopping processes on vault mount %s\n", mount_point);
    kill_pids_from_fuser(mount_point, 0);
    usleep(400000);
    kill_pids_from_fuser(mount_point, 1);
    return 0;
}

static int unmount_mount_point(const char *mount_point)
{
    const char *umount_args[] = {"umount", mount_point, NULL};
    return run_cmd(umount_args);
}

static int unmount_mount_point_lazy(const char *mount_point)
{
    const char *umount_args[] = {"umount", "-l", mount_point, NULL};
    return run_cmd(umount_args);
}

static int path_has_our_mapper_mount(const char *mount_point)
{
    return is_mountpoint_path(mount_point)
        && mount_source_matches_mapper(mount_point, MAPPER_PATH);
}

static int unmount_our_path_robust(const char *mount_point, int allow_kill)
{
    if (!mount_point || !*mount_point)
        return 0;
    if (!is_mountpoint_path(mount_point))
        return 0;
    if (!mount_source_matches_mapper(mount_point, MAPPER_PATH)) {
        fprintf(stderr,
                "[vault_manager] Refusing umount: %s is not mounted from %s\n",
                mount_point, MAPPER_PATH);
        return -1;
    }

    sync();
    if (unmount_mount_point(mount_point) == 0)
        return 0;

    if (allow_kill) {
        fprintf(stderr, "[vault_manager] Mount busy on %s — stopping holders\n", mount_point);
        kill_pids_from_fuser(mount_point, 0);
        usleep(400000);
        kill_pids_from_fuser(mount_point, 1);
        usleep(300000);
        if (unmount_mount_point(mount_point) == 0)
            return 0;
    }

    if (unmount_mount_point_lazy(mount_point) == 0) {
        fprintf(stderr, "[vault_manager] Lazy-unmounted %s\n", mount_point);
        return 0;
    }

    report_mount_busy(mount_point, MAPPER_PATH);
    return -1;
}

static int mapper_still_open(void)
{
    return mapper_exists(MAPPER_PATH);
}

static int vault_runtime_reconcile(const char *user_mount_point)
{
    int rc = 0;

    if (path_has_our_mapper_mount(SETUP_TMPDIR)) {
        if (unmount_our_path_robust(SETUP_TMPDIR, 1) != 0)
            rc = 1;
    }

    if (user_mount_point && user_mount_point[0] && path_has_our_mapper_mount(user_mount_point)) {
        if (unmount_our_path_robust(user_mount_point, 1) != 0)
            rc = 1;
    }

    if (mapper_exists(MAPPER_PATH)) {
        if (close_mapper() != 0)
            rc = 1;
    }

    if (mapper_still_open())
        rc = 1;

    return rc;
}

static int lock_vault_after_unmount(const char *mount_point, const char *mapper_path, int allow_kill)
{
    if (unmount_mount_point(mount_point) == 0)
        return close_mapper_path(mapper_path);

    if (!allow_kill) {
        report_mount_busy(mount_point, mapper_path);
        return -1;
    }

    fprintf(stderr, "[vault_manager] Vault mount busy — force-stopping holders\n");
    if (kill_mount_holders(mount_point, mapper_path) != 0)
        return -1;

    usleep(300000);
    if (unmount_mount_point(mount_point) != 0) {
        report_mount_busy(mount_point, mapper_path);
        return -1;
    }

    return close_mapper_path(mapper_path);
}

static int finalize_setup_mapper(int had_temp_mount)
{
    (void)had_temp_mount;
    if (vault_runtime_reconcile(NULL) != 0) {
        fprintf(stderr, "[vault_manager] Setup cleanup: failed to release vault mapper\n");
        return 1;
    }
    return 0;
}

static void setup_abort_cleanup(void)
{
    vault_runtime_reconcile(NULL);
}

/* ─── Commands ───────────────────────────────────────────────────────────── */

/**
 * Diskin mevcut durumunu kontrol et.
 * Üzerinde bir LUKS konteyneri veya dosya sistemi varsa uyarı ver.
 *
 * Çıkış kodları:
 *   0  = Disk boş, güvenle formatlanabilir
 *   1  = Üzerinde mevcut LUKS konteyneri var (bizim vault)
 *   2  = Üzerinde başka bir dosya sistemi var (veri kaybı riski!)
 */
static int check_device_status(const char *device)
{
    printf("[vault_manager] Scanning device: %s\n", device);

    /* 1. Önce LUKS mu diye kontrol et */
    const char *isluks_args[] = {"cryptsetup", "isLuks", device, NULL};
    int is_luks = run_cmd(isluks_args);

    if (is_luks == 0) {
        printf("[vault_manager] STATUS: Device already contains a LUKS container.\n");
        return 1;  /* Bizim vault olabilir, ayrıca uyar */
    }

    /* 2. Başka bir dosya sistemi var mı? (blkid çıktısını kontrol et) */
    int pipefd[2];
    if (pipe(pipefd) < 0) return 0; /* pipe açılamazsa güvenli devam et */

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execlp("blkid", "blkid", "-o", "value", "-s", "TYPE", device, NULL);
        _exit(1);
    }

    close(pipefd[1]);
    char buf[256] = {0};
    read(pipefd[0], buf, sizeof(buf) - 1);
    close(pipefd[0]);
    int status;
    waitpid(pid, &status, 0);

    /* blkid bir şey döndürdüyse dosya sistemi var demektir */
    if (strlen(buf) > 1) {
        /* Satır sonundaki newline'ı temizle */
        buf[strcspn(buf, "\n")] = '\0';
        printf("[vault_manager] STATUS: Device contains existing filesystem: %s\n", buf);
        printf("[vault_manager] WARNING: Formatting will DESTROY all existing data!\n");
        return 2;  /* Veri var, tehlikeli! */
    }

    printf("[vault_manager] STATUS: Device appears empty. Safe to format.\n");
    return 0;  /* Boş disk, güvenle devam et */
}

/**
 * SETUP: İlk kullanım — LUKS formatla + ext4 oluştur.
 *
 * Akış:
 *   0. Disk durumunu kontrol et
 *   1. Parmak izi doğrula → key al
 *   2. cryptsetup luksFormat (AES-256-XTS)
 *   3. cryptsetup open
 *   4. mkfs.ext4
 *   5. Mount → chown → unmount
 *   6. cryptsetup close
 */
static int cmd_setup(const char *device, int finger_id, int force, int partition_size_mb,
                     int key_from_stdin,
                     const char *vault_name, const char *normal_name, const char *normal_fs)
{
    if (!vault_name) vault_name = "DigitalVault";
    if (!normal_name) normal_name = "Normal";
    if (!normal_fs) normal_fs = "exfat";

    printf("[vault_manager] ═══════════════════════════════════════\n");
    printf("[vault_manager] Setting up new vault on: %s %s\n", device, force ? "(FORCED)" : "");
    printf("[vault_manager]   Vault Name  : %s\n", vault_name);
    if (partition_size_mb > 0) {
        printf("[vault_manager]   Normal Name : %s\n", normal_name);
        printf("[vault_manager]   Normal FS   : %s\n", normal_fs);
    }
    printf("[vault_manager] ═══════════════════════════════════════\n");

    /* 0. Disk güvenlik kontrolü */
    int disk_status = check_device_status(device);
    if (!force) {
        if (disk_status == 1) {
            printf("[vault_manager] INFO: Device already has a LUKS vault.\n");
            printf("[vault_manager] INFO: Use 'open' to unlock it, or pass --force to reformat.\n");
            return 3;  /* GUI: "Bu cihazda zaten bir vault var" */
        }
        if (disk_status == 2) {
            fprintf(stderr, "[vault_manager] ABORT: Existing data detected!\n");
            fprintf(stderr, "[vault_manager] ABORT: Use --force flag to override (DATA WILL BE LOST).\n");
            return 4;  /* GUI: "Bu cihazda veri var, silmek istediğinden emin misin?" */
        }
    } else {
        printf("[vault_manager] WARNING: Force flag detected. Overwriting existing data...\n");
    }

    char target_device[256];
    strncpy(target_device, device, sizeof(target_device));

    /* 0.5 Partitioning (Optional) */
    if (partition_size_mb > 0) {
        printf("[vault_manager] Partitioning device: %s (Vault: %d MB)\n", device, partition_size_mb);
        
        /* 1. Wipe existing signatures */
        const char *wipe_args[] = {"wipefs", "-a", device, NULL};
        run_cmd(wipe_args);

        /* 2. Create partition table */
        const char *label_args[] = {"parted", "-s", device, "mklabel", "gpt", NULL};
        if (run_cmd(label_args) != 0) {
            fprintf(stderr, "[vault_manager] Failed to create partition table!\n");
            return 1;
        }

        /* 3. Create vault partition (Partition 1) */
        char size_str[32];
        snprintf(size_str, sizeof(size_str), "%dMiB", partition_size_mb);
        const char *part1_args[] = {"parted", "-s", device, "mkpart", "primary", "ext4", "1MiB", size_str, NULL};
        if (run_cmd(part1_args) != 0) {
            fprintf(stderr, "[vault_manager] Failed to create vault partition!\n");
            return 1;
        }

        /* 4. Create normal partition (Partition 2) */
        const char *part2_args[] = {"parted", "-s", device, "mkpart", "primary", "fat32", size_str, "100%", NULL};
        if (run_cmd(part2_args) != 0) {
            fprintf(stderr, "[vault_manager] Failed to create normal partition!\n");
            return 1;
        }

        /* 5. Format partition 2 as exFAT */
        printf("[vault_manager] Waiting for partitions to settle...\n");
        usleep(1000000); // 1.0s for kernel to refresh
        
        char p1[270], p2[270];
        // Handle loop device naming (loop0p1) vs disk naming (sdb1)
        if (strstr(device, "loop")) {
            snprintf(p1, sizeof(p1), "%sp1", device);
            snprintf(p2, sizeof(p2), "%sp2", device);
        } else {
            snprintf(p1, sizeof(p1), "%s1", device);
            snprintf(p2, sizeof(p2), "%s2", device);
        }

        printf("[vault_manager] Formatting normal partition: %s (%s)\n", p2, normal_fs);
        if (strcmp(normal_fs, "exfat") == 0) {
            const char *mkexfat_args[] = {"mkfs.exfat", "-L", normal_name, p2, NULL};
            if (run_cmd(mkexfat_args) != 0) {
                fprintf(stderr, "[vault_manager] Warning: exFAT format failed. Falling back to FAT32.\n");
                const char *mkfat_args[] = {"mkfs.vfat", "-n", normal_name, p2, NULL};
                run_cmd(mkfat_args);
            }
        } else if (strcmp(normal_fs, "vfat") == 0 || strcmp(normal_fs, "fat32") == 0) {
            const char *mkfat_args[] = {"mkfs.vfat", "-n", normal_name, p2, NULL};
            run_cmd(mkfat_args);
        } else if (strcmp(normal_fs, "ext4") == 0) {
            const char *mkext4_args[] = {"mkfs.ext4", "-L", normal_name, p2, NULL};
            run_cmd(mkext4_args);
        } else {
            fprintf(stderr, "[vault_manager] Unknown filesystem type: %s. Using exFAT.\n", normal_fs);
            const char *mkexfat_args[] = {"mkfs.exfat", "-L", normal_name, p2, NULL};
            run_cmd(mkexfat_args);
        }

        // Update target for LUKS
        strncpy(target_device, p1, sizeof(target_device));
        printf("[vault_manager] Partitioning complete. Vault target: %s\n", target_device);
    }

    /* 1. Biyometrik key al */
    uint8_t key[HAL_KEY_LEN];
    if (key_from_stdin) {
        if (read_key_from_stdin(key) != 0)
            return 1;
    } else if (get_biometric_key(finger_id, key) != 0) {
        return 1;
    }

    /* 2. LUKS formatla */
    printf("[vault_manager] Formatting LUKS2 container (AES-256-XTS)...\n");
    const char *fmt_args[] = {
        "cryptsetup", "luksFormat",
        "--type",       "luks2",
        "--cipher",     "aes-xts-plain64",
        "--key-size",   "512",           /* 512 bit = AES-256-XTS (2×256) */
        "--hash",       "sha256",
        "--key-file=-",
        "--keyfile-size", "32",
        "--batch-mode",
        target_device, NULL
    };

    if (cryptsetup_with_key(fmt_args, key, HAL_KEY_LEN) != 0) {
        fprintf(stderr, "[vault_manager] LUKS format failed!\n");
        secure_wipe(key, HAL_KEY_LEN);
        return 1;
    }

    /* 3. LUKS aç */
    printf("[vault_manager] Opening LUKS container...\n");
    const char *open_args[] = {
        "cryptsetup", "open",
        "--type",       "luks2",
        "--key-file=-",
        "--keyfile-size", "32",
        target_device, MAPPER_NAME, NULL
    };

    if (cryptsetup_with_key(open_args, key, HAL_KEY_LEN) != 0) {
        fprintf(stderr, "[vault_manager] LUKS open failed!\n");
        secure_wipe(key, HAL_KEY_LEN);
        setup_abort_cleanup();
        return 1;
    }

    /* Key artık gerekli değil — bellekten sil */
    secure_wipe(key, HAL_KEY_LEN);
    printf("[vault_manager] Encryption key wiped from memory.\n");

    /* 4. ext4 dosya sistemi oluştur */
    printf("[vault_manager] Creating ext4 filesystem...\n");
    const char *mkfs_args[] = {
        "mkfs.ext4",
        "-L", vault_name,           /* Disk etiketi */
        "-m", "1",                  /* %1 reserved (varsayılan %5 — USB için gereksiz) */
        MAPPER_PATH, NULL
    };

    if (run_cmd(mkfs_args) != 0) {
        fprintf(stderr, "[vault_manager] ext4 format failed!\n");
        setup_abort_cleanup();
        return 1;
    }

    /* 5. Geçici mount → kullanıcı izinlerini ayarla → unmount */
    int had_temp_mount = 0;
    mkdir(SETUP_TMPDIR, 0755);

    const char *mount_args[] = {"mount", MAPPER_PATH, SETUP_TMPDIR, NULL};
    if (run_cmd(mount_args) == 0) {
        had_temp_mount = 1;
        const char *sudo_uid = getenv("SUDO_UID");
        const char *sudo_gid = getenv("SUDO_GID");

        if (sudo_uid && sudo_gid) {
            const char *chown_args[] = {
                "chown", "-R",
                NULL, SETUP_TMPDIR, NULL
            };
            char ownership[64];
            snprintf(ownership, sizeof(ownership), "%s:%s", sudo_uid, sudo_gid);
            chown_args[2] = ownership;
            run_cmd(chown_args);
        } else {
            chmod(SETUP_TMPDIR, 0777);
        }
    }

    if (finalize_setup_mapper(had_temp_mount) != 0) {
        fprintf(stderr, "[vault_manager] Setup failed: vault mapper was left open.\n");
        rmdir(SETUP_TMPDIR);
        return 1;
    }

    rmdir(SETUP_TMPDIR);

    printf("[vault_manager] ═══════════════════════════════════════\n");
    printf("[vault_manager] Vault setup complete!\n");
    printf("[vault_manager]   Device : %s\n", target_device);
    printf("[vault_manager]   Cipher : AES-256-XTS (LUKS2)\n");
    printf("[vault_manager]   FS     : ext4\n");
    printf("[vault_manager]   Label  : %s\n", vault_name);
    printf("[vault_manager] ═══════════════════════════════════════\n");
    return 0;
}

/**
 * OPEN: Parmak izi doğrula → LUKS aç → ext4 mount.
 */
static int cmd_open(const char *device, const char *mount_point, int finger_id, int key_from_stdin)
{
    char resolved_device[256];
    resolve_vault_device(device, resolved_device, sizeof(resolved_device));

    /* Zaten açık mı? */
    if (access(MAPPER_PATH, F_OK) == 0) {
        fprintf(stderr, "[vault_manager] Vault is already open!\n");
        return 1;
    }

    printf("[vault_manager] Opening vault: %s → %s\n", resolved_device, mount_point);

    /* 1. Biyometrik key al */
    uint8_t key[HAL_KEY_LEN];
    if (key_from_stdin) {
        if (read_key_from_stdin(key) != 0)
            return 1;
    } else if (get_biometric_key(finger_id, key) != 0) {
        return 1;
    }

    /* 2. LUKS aç */
    printf("[vault_manager] Decrypting LUKS container...\n");
    const char *open_args[] = {
        "cryptsetup", "open",
        "--type",       "luks2",
        "--key-file=-",
        "--keyfile-size", "32",
        resolved_device, MAPPER_NAME, NULL
    };

    if (cryptsetup_with_key(open_args, key, HAL_KEY_LEN) != 0) {
        fprintf(stderr, "[vault_manager] LUKS open failed! Wrong fingerprint or not a LUKS device.\n");
        secure_wipe(key, HAL_KEY_LEN);
        return 1;
    }

    /* Key bellekten sil */
    secure_wipe(key, HAL_KEY_LEN);
    printf("[vault_manager] Encryption key wiped from memory.\n");

    /* 3. Mount noktası oluştur */
    mkdir(mount_point, 0755);

    /* 4. ext4 mount et */
    printf("[vault_manager] Mounting filesystem at %s\n", mount_point);
    const char *mount_args[] = {"mount", MAPPER_PATH, mount_point, NULL};

    if (run_cmd(mount_args) != 0) {
        fprintf(stderr, "[vault_manager] Mount failed!\n");
        const char *close[] = {"cryptsetup", "close", MAPPER_NAME, NULL};
        run_cmd(close);
        return 1;
    }

    /* 5. Kullanıcı izinlerini ayarla (sudo/pkexec ile çalıştırıldıysa) */
    const char *sudo_uid = getenv("SUDO_UID");
    const char *sudo_gid = getenv("SUDO_GID");
    if (sudo_uid && sudo_gid) {
        uid_t uid = (uid_t)atoi(sudo_uid);
        gid_t gid = (gid_t)atoi(sudo_gid);

        /* Mount noktasının sahibini gerçek kullanıcıya ayarla */
        if (chown(mount_point, uid, gid) != 0) {
            fprintf(stderr, "[vault_manager] Warning: chown failed: %s\n",
                    strerror(errno));
        }

        /* İçerideki mevcut dosyaları da chown et */
        char chown_cmd[512];
        snprintf(chown_cmd, sizeof(chown_cmd), "%s:%s", sudo_uid, sudo_gid);
        const char *chown_args[] = {
            "chown", "-R", chown_cmd, mount_point, NULL
        };
        run_cmd(chown_args);

        printf("[vault_manager] File permissions set for UID=%s GID=%s\n",
               sudo_uid, sudo_gid);
    }

    printf("[vault_manager] ✓ Vault is now accessible at %s\n", mount_point);
    printf("[vault_manager] ✓ You can access files via file manager or terminal:\n");
    printf("[vault_manager]     ls %s\n", mount_point);
    printf("[vault_manager]     cd %s\n", mount_point);
    return 0;
}

/**
 * CLOSE: Normal unmount, then cryptsetup close. Does not kill processes.
 * Returns EXIT_CLOSE_MOUNT_BUSY when the mount point is in use.
 */
static int cmd_close(const char *mount_point)
{
    printf("[vault_manager] Locking vault...\n");

    const VaultRuntimeState st = probe_vault_state(mount_point, MAPPER_PATH);

    switch (st) {
    case VAULT_STATE_CLEAN:
        printf("[vault_manager] Vault is already locked.\n");
        return 0;
    case VAULT_STATE_MAPPER_ONLY:
        printf("[vault_manager] Mapper open without mount — closing mapper.\n");
        if (close_mapper() != 0) {
            fprintf(stderr, "[vault_manager] Could not close mapper.\n");
            return 1;
        }
        printf("[vault_manager] Vault locked.\n");
        return 0;
    case VAULT_STATE_MOUNTED_FOREIGN:
        fprintf(stderr,
                "[vault_manager] Mount point %s is in use by another filesystem; refusing to lock.\n",
                mount_point);
        return 1;
    case VAULT_STATE_MOUNTED_OURS:
        printf("[vault_manager] Unmounting %s\n", mount_point);
        if (unmount_mount_point(mount_point) != 0) {
            report_mount_busy(mount_point, MAPPER_PATH);
            return EXIT_CLOSE_MOUNT_BUSY;
        }
        printf("[vault_manager] Closing encrypted vault mapper...\n");
        if (close_mapper() != 0) {
            fprintf(stderr, "[vault_manager] Could not close mapper (device may still be busy).\n");
            return 1;
        }
        printf("[vault_manager] Vault locked and encryption keys wiped from memory.\n");
        return 0;
    default:
        return 1;
    }
}

/**
 * FORCECLOSE: Terminate processes holding the verified vault mount, unmount, close mapper.
 * Used for HAL loss, app shutdown, and startup recovery.
 */
static int cmd_forceclose(const char *mount_point)
{
    printf("[vault_manager] Force-locking vault...\n");
    if (vault_runtime_reconcile(mount_point) != 0) {
        fprintf(stderr, "[vault_manager] Force close failed.\n");
        return 1;
    }
    printf("[vault_manager] Vault force-locked.\n");
    return 0;
}

static int cmd_reconcile(const char *mount_point)
{
    printf("[vault_manager] Reconciling vault runtime state...\n");
    if (vault_runtime_reconcile(mount_point) != 0) {
        fprintf(stderr, "[vault_manager] Reconcile incomplete.\n");
        return 1;
    }
    printf("RECONCILE_OK\n");
    return 0;
}

static int cmd_deletevault(const char *vault_partition)
{
    if (!vault_partition || !*vault_partition) {
        fprintf(stderr, "[vault_manager] deletevault: missing partition path\n");
        return 1;
    }

    printf("[vault_manager] Deleting encrypted vault on %s\n", vault_partition);

    if (vault_runtime_reconcile(NULL) != 0) {
        fprintf(stderr, "[vault_manager] deletevault: could not release mapper/mounts\n");
        return 1;
    }

    const char *wipe_args[] = {"wipefs", "-a", vault_partition, NULL};
    if (run_cmd(wipe_args) != 0) {
        fprintf(stderr, "[vault_manager] deletevault: wipefs failed on %s\n", vault_partition);
        return 1;
    }

    printf("DELETED_VAULT:%s\n", vault_partition);
    return 0;
}

/**
 * cryptsetup luksAddKey için iki anahtarı (existing + new) sırayla pipe'la.
 *
 * luksAddKey stdin'den şunu bekler:
 *   - Önce mevcut anahtar (existing key) + newline
 *   - Sonra yeni anahtar (new key) + newline
 *
 * @param argv      cryptsetup argümanları (NULL-terminated)
 * @param existing  Mevcut sahip anahtarı
 * @param new_key   Yeni kullanıcı anahtarı
 * @param key_len   Her iki anahtarın boyutu
 * @return 0 başarılı, -1 hata
 */
static int cryptsetup_addkey_pipe(const char *argv[],
                                  const uint8_t *existing, 
                                  const uint8_t *new_key,
                                  size_t key_len)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("[vault_manager] pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("[vault_manager] fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execvp("cryptsetup", (char *const *)argv);
        perror("[vault_manager] execvp cryptsetup");
        _exit(127);
    }

    /* Parent: iki anahtarı sırasıyla yaz */
    close(pipefd[0]);
    write(pipefd[1], existing, key_len);
    write(pipefd[1], new_key, key_len);
    close(pipefd[1]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }

    fprintf(stderr, "[vault_manager] cryptsetup luksAddKey exited with code %d\n",
            WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    return -1;
}

/**
 * ADDKEY: Mevcut bir LUKS vault'a yeni bir kullanıcı (keyslot) ekle.
 *
 * Akış:
 *   1. Sahip parmak izi doğrula → mevcut key al
 *   2. Yeni kullanıcı parmak izi doğrula → yeni key al
 *   3. cryptsetup luksAddKey ile yeni keyslot oluştur
 */
static int cmd_addkey(const char *device, int owner_fid, int new_fid)
{
    char resolved_device[256];
    resolve_vault_device(device, resolved_device, sizeof(resolved_device));

    printf("[vault_manager] ═══════════════════════════════════════\n");
    printf("[vault_manager] Adding new access to: %s\n", resolved_device);
    printf("[vault_manager] ═══════════════════════════════════════\n");

    /* 1. Sahip doğrulaması */
    printf("[vault_manager] Step 1: Verifying vault owner (finger ID: %d)...\n", owner_fid);
    uint8_t owner_key[HAL_KEY_LEN];
    if (get_biometric_key(owner_fid, owner_key) != 0) {
        fprintf(stderr, "[vault_manager] Owner verification failed!\n");
        return 1;
    }
    printf("[vault_manager] ✓ Owner verified.\n");

    /* 2. Yeni kullanıcı kaydı */
    printf("[vault_manager] Step 2: Scan new user's fingerprint (finger ID: %d)...\n", new_fid);
    uint8_t new_key[HAL_KEY_LEN];
    if (get_biometric_key(new_fid, new_key) != 0) {
        fprintf(stderr, "[vault_manager] New user fingerprint failed!\n");
        secure_wipe(owner_key, HAL_KEY_LEN);
        return 1;
    }
    printf("[vault_manager] ✓ New user fingerprint captured.\n");

    /* 3. luksAddKey çağır */
    printf("[vault_manager] Adding new keyslot...\n");
    const char *addkey_args[] = {
        "cryptsetup", "luksAddKey",
        "--key-file", "-",
        "--new-keyfile-size", "32",
        "--key-file-size", "32",
        resolved_device,
        "-",  /* new key de stdin'den */
        NULL
    };

    int ret = cryptsetup_addkey_pipe(addkey_args, owner_key, new_key, HAL_KEY_LEN);

    secure_wipe(owner_key, HAL_KEY_LEN);
    secure_wipe(new_key, HAL_KEY_LEN);

    if (ret != 0) {
        fprintf(stderr, "[vault_manager] Failed to add keyslot!\n");
        return 1;
    }

    printf("[vault_manager] ✓ New access granted successfully!\n");
    return 0;
}

/**
 * KILLSLOT: Belirli bir keyslot'u kaldır (erişimi iptal et).
 *
 * Akış:
 *   1. Sahip parmak izi doğrula
 *   2. cryptsetup luksKillSlot ile belirtilen slot'u sil
 */
static int cmd_killslot(const char *device, int owner_fid, int slot_num)
{
    char resolved_device[256];
    resolve_vault_device(device, resolved_device, sizeof(resolved_device));

    printf("[vault_manager] ═══════════════════════════════════════\n");
    printf("[vault_manager] Revoking access slot %d on: %s\n", slot_num, resolved_device);
    printf("[vault_manager] ═══════════════════════════════════════\n");

    /* Slot 0'ı silmeye izin verme — sahip her zaman kalmalı */
    if (slot_num == 0) {
        fprintf(stderr, "[vault_manager] ERROR: Cannot remove slot 0 (vault owner)!\n");
        return 5;
    }

    /* 1. Sahip doğrulaması */
    printf("[vault_manager] Verifying vault owner...\n");
    uint8_t owner_key[HAL_KEY_LEN];
    if (get_biometric_key(owner_fid, owner_key) != 0) {
        fprintf(stderr, "[vault_manager] Owner verification failed!\n");
        return 1;
    }
    secure_wipe(owner_key, HAL_KEY_LEN);
    printf("[vault_manager] ✓ Owner verified.\n");

    /* 2. Keyslot'u kaldır */
    printf("[vault_manager] Killing keyslot %d...\n", slot_num);
    char slot_str[8];
    snprintf(slot_str, sizeof(slot_str), "%d", slot_num);

    const char *kill_args[] = {
        "cryptsetup", "luksKillSlot", "-q", resolved_device, slot_str, NULL
    };

    if (run_cmd(kill_args) != 0) {
        fprintf(stderr, "[vault_manager] Failed to remove keyslot %d!\n", slot_num);
        return 1;
    }

    printf("[vault_manager] ✓ Access slot %d revoked.\n", slot_num);
    return 0;
}

/**
 * LISTSLOTS: LUKS header'daki aktif keyslot'ları listele.
 * Çıktı satır formatı: SLOT:<num>:<state>
 * Böylece GUI çıktıyı kolayca parse edebilir.
 */
static int cmd_listslots(const char *device)
{
    char resolved_device[256];
    resolve_vault_device(device, resolved_device, sizeof(resolved_device));

    printf("[vault_manager] Scanning keyslots on: %s\n", resolved_device);

    /* cryptsetup luksDump çıktısını oku ve keyslot satırlarını filtrele */
    int pipefd[2];
    if (pipe(pipefd) < 0) return 1;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execlp("cryptsetup", "cryptsetup", "luksDump", resolved_device, NULL);
        _exit(127);
    }

    close(pipefd[1]);
    char buf[8192] = {0};
    ssize_t total = 0;
    ssize_t n;
    while ((n = read(pipefd[0], buf + total, sizeof(buf) - total - 1)) > 0) {
        total += n;
    }
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    /* Parse: "  N: luks2" pattern'ini ara */
    char *line = strtok(buf, "\n");
    int in_keyslots = 0;
    while (line) {
        if (strstr(line, "Keyslots:")) {
            in_keyslots = 1;
        } else if (in_keyslots) {
            int slot_num;
            char state[64];
            /* "  0: luks2" formatını yakala */
            if (sscanf(line, " %d: %63s", &slot_num, state) == 2) {
                printf("SLOT:%d:%s\n", slot_num, state);
            } else if (line[0] != ' ' && line[0] != '\t') {
                /* Keyslots bölümünden çıktık */
                break;
            }
        }
        line = strtok(NULL, "\n");
    }

    return 0;
}

/**
 * STATUS: Vault durumunu kontrol et.
 * Çıktı: "OPEN" veya "CLOSED" (GUI tarafından parse edilir)
 */
static int cmd_status(void)
{
    const int mapper_open = mapper_exists(MAPPER_PATH);
    const int setup_mounted = path_has_our_mapper_mount(SETUP_TMPDIR);

    if (!mapper_open && !setup_mounted) {
        printf("CLOSED\n");
        return 1;
    }
    if (setup_mounted) {
        printf("SETUP_ORPHAN\n");
        return 0;
    }
    printf("OPEN_MAPPER_ONLY\n");
    return 0;
}

/* ─── CREATELOOP: sparse image + losetup ─────────────────────────────────── */

static int mkdir_parents(const char *file_path)
{
    char dir[512];
    strncpy(dir, file_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *slash = strrchr(dir, '/');
    if (!slash) return 0;
    *slash = '\0';
    if (dir[0] == '\0') return 0;

    char tmp[512];
    strncpy(tmp, dir, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int cmd_createloop(const char *image_path, int size_mb)
{
    if (!image_path || size_mb < 16) {
        fprintf(stderr, "[vault_manager] createloop: invalid path or size (min 16 MB)\n");
        return 1;
    }

    if (access(image_path, F_OK) == 0) {
        fprintf(stderr, "[vault_manager] createloop: image already exists: %s\n", image_path);
        return 2;
    }

    if (mkdir_parents(image_path) != 0) {
        perror("[vault_manager] createloop: mkdir");
        return 1;
    }

    char size_arg[32];
    snprintf(size_arg, sizeof(size_arg), "%dM", size_mb);
    const char *truncate_args[] = {"truncate", "-s", size_arg, image_path, NULL};
    if (run_cmd(truncate_args) != 0) {
        fprintf(stderr, "[vault_manager] createloop: truncate failed\n");
        return 1;
    }

    char cmd[768];
    snprintf(cmd, sizeof(cmd), "losetup -f --show '%s'", image_path);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("[vault_manager] createloop: popen");
        return 1;
    }

    char loop_dev[128] = {0};
    if (!fgets(loop_dev, sizeof(loop_dev), fp)) {
        pclose(fp);
        fprintf(stderr, "[vault_manager] createloop: losetup produced no output\n");
        return 1;
    }
    pclose(fp);

    loop_dev[strcspn(loop_dev, "\n")] = '\0';
    if (strncmp(loop_dev, "/dev/loop", 9) != 0) {
        fprintf(stderr, "[vault_manager] createloop: unexpected losetup output: %s\n", loop_dev);
        return 1;
    }

    printf("%s\n", loop_dev);
    printf("[vault_manager] createloop: %s -> %s (%d MB)\n", image_path, loop_dev, size_mb);
    return 0;
}

static int backing_path_allowed(const char *backing_path)
{
    if (!backing_path || backing_path[0] != '/')
        return 0;

    size_t len = strlen(backing_path);
    if (len < 5 || strcmp(backing_path + len - 4, ".img") != 0)
        return 0;

    if (strstr(backing_path, "/DigitalVault/containers/") != NULL)
        return 1;
    if (strstr(backing_path, "/.local/share/DigitalVault/containers/") != NULL)
        return 1;

    return 0;
}

/**
 * DESTROYLOOP: Detach loop device and delete backing sparse image.
 * Backing path is supplied by the GUI registry (not parsed from losetup -J).
 */
static int cmd_destroyloop(const char *device_path, const char *backing_path)
{
    if (!device_path || strncmp(device_path, "/dev/loop", 9) != 0) {
        fprintf(stderr, "[vault_manager] destroyloop: invalid loop device path\n");
        return 1;
    }

    if (!backing_path || backing_path[0] == '\0') {
        fprintf(stderr, "[vault_manager] destroyloop: missing backing image path\n");
        return 2;
    }

    if (!backing_path_allowed(backing_path)) {
        fprintf(stderr, "[vault_manager] destroyloop: backing path not allowed: %s\n",
                backing_path);
        return 2;
    }

    if (access(MAPPER_PATH, F_OK) == 0) {
        fprintf(stderr, "[vault_manager] destroyloop: vault is open (%s)\n", MAPPER_PATH);
        return 1;
    }

    const char *detach_args[] = {"losetup", "-d", device_path, NULL};
    if (run_cmd(detach_args) != 0) {
        fprintf(stderr, "[vault_manager] destroyloop: losetup -d failed\n");
        return 1;
    }

    if (unlink(backing_path) != 0) {
        fprintf(stderr, "[vault_manager] destroyloop: could not delete %s: %s\n",
                backing_path, strerror(errno));
        return 3;
    }

    printf("DELETED:%s\n", backing_path);
    printf("[vault_manager] destroyloop: removed %s and detached %s\n",
           backing_path, device_path);
    return 0;
}

/* ─── Main ───────────────────────────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "DigitalVault — Vault Manager (LUKS + ext4)\n"
        "\n"
        "Usage:\n"
        "  %s setup     <device> [finger_id] [--key-from-stdin] — First-time LUKS setup\n"
        "  %s open      <device> <mountpoint> [--key-from-stdin] — Unlock vault\n"
        "  %s close     <mountpoint>                        — Lock vault (no process kill)\n"
        "  %s forceclose <mountpoint>                      — Force lock vault\n"
        "  %s reconcile <mountpoint>                        — Release orphan mapper/setup mounts\n"
        "  %s deletevault <luks_partition>                  — Wipe encrypted vault partition\n"
        "  %s createloop <image_path> <size_mb>             — Create sparse image + loop device\n"
        "  %s destroyloop <device_path> <backing_path>      — Detach loop + delete backing image\n"
        "  %s addkey    <device> <owner_fid> <new_fid>      — Grant access\n"
        "  %s killslot  <device> <owner_fid> <slot_num>     — Revoke access\n"
        "  %s listslots <device>                            — List active keyslots\n"
        "  %s status                                        — Check vault status\n"
        "\n",
        prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "setup") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s setup <device> [finger_id] [--force]\n", argv[0]);
            return 1;
        }
        int fid = 1;
        int force = 0;
        int key_from_stdin = 0;
        int partition_size = 0;
        const char *vault_name = "DigitalVault";
        const char *normal_name = "Normal";
        const char *normal_fs = "exfat";

        /* Parse optional arguments */
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--key-from-stdin") == 0) {
                key_from_stdin = 1;
            } else if (strcmp(argv[i], "--force") == 0) {
                force = 1;
            } else if (strcmp(argv[i], "--partition") == 0 && i + 1 < argc) {
                partition_size = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--vault-name") == 0 && i + 1 < argc) {
                vault_name = argv[++i];
            } else if (strcmp(argv[i], "--normal-name") == 0 && i + 1 < argc) {
                normal_name = argv[++i];
            } else if (strcmp(argv[i], "--normal-fs") == 0 && i + 1 < argc) {
                normal_fs = argv[++i];
            } else {
                fid = atoi(argv[i]);
            }
        }

        return cmd_setup(argv[2], fid, force, partition_size, key_from_stdin,
                         vault_name, normal_name, normal_fs);
    }
    else if (strcmp(cmd, "open") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s open <device> <mountpoint> [--key-from-stdin]\n", argv[0]);
            return 1;
        }
        int fid = 1;
        int key_from_stdin = 0;
        for (int i = 4; i < argc; i++) {
            if (strcmp(argv[i], "--key-from-stdin") == 0)
                key_from_stdin = 1;
            else
                fid = atoi(argv[i]);
        }
        return cmd_open(argv[2], argv[3], fid, key_from_stdin);
    }
    else if (strcmp(cmd, "close") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s close <mountpoint>\n", argv[0]);
            return 1;
        }
        return cmd_close(argv[2]);
    }
    else if (strcmp(cmd, "forceclose") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s forceclose <mountpoint>\n", argv[0]);
            return 1;
        }
        return cmd_forceclose(argv[2]);
    }
    else if (strcmp(cmd, "reconcile") == 0) {
        const char *mp = (argc >= 3) ? argv[2] : "/tmp";
        return cmd_reconcile(mp);
    }
    else if (strcmp(cmd, "deletevault") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s deletevault <luks_partition>\n", argv[0]);
            return 1;
        }
        return cmd_deletevault(argv[2]);
    }
    else if (strcmp(cmd, "addkey") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s addkey <device> <owner_fid> <new_fid>\n", argv[0]);
            return 1;
        }
        return cmd_addkey(argv[2], atoi(argv[3]), atoi(argv[4]));
    }
    else if (strcmp(cmd, "killslot") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s killslot <device> <owner_fid> <slot_num>\n", argv[0]);
            return 1;
        }
        return cmd_killslot(argv[2], atoi(argv[3]), atoi(argv[4]));
    }
    else if (strcmp(cmd, "listslots") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s listslots <device>\n", argv[0]);
            return 1;
        }
        return cmd_listslots(argv[2]);
    }
    else if (strcmp(cmd, "status") == 0) {
        return cmd_status();
    }
    else if (strcmp(cmd, "createloop") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s createloop <image_path> <size_mb>\n", argv[0]);
            return 1;
        }
        return cmd_createloop(argv[2], atoi(argv[3]));
    }
    else if (strcmp(cmd, "destroyloop") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s destroyloop <device_path> <backing_path>\n", argv[0]);
            return 1;
        }
        return cmd_destroyloop(argv[2], argv[3]);
    }
    else {
        fprintf(stderr, "[vault_manager] Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
