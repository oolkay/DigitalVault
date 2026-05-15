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

#include "hal.h"

/* ─── Constants ──────────────────────────────────────────────────────────── */

#define MAPPER_NAME     "digitalvault"
#define MAPPER_PATH     "/dev/mapper/" MAPPER_NAME
#define HAL_KEY_LEN     32      /* HAL'dan gelen master key boyutu */
#define HAL_SOCKET      "simulator:/tmp/biometric_vault.sock"

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
static int get_biometric_key(int finger_id, uint8_t *key_out)
{
    printf("[vault_manager] Connecting to biometric sensor...\n");

    if (hal_init(HAL_SOCKET) != HAL_OK) {
        fprintf(stderr, "[vault_manager] HAL connection failed!\n");
        fprintf(stderr, "[vault_manager] Is the simulator running?\n");
        return -1;
    }

    printf("[vault_manager] Verifying fingerprint (ID: %d)...\n", finger_id);

    int ret = hal_verify_finger(finger_id, key_out);
    if (ret != HAL_OK) {
        fprintf(stderr, "[vault_manager] Fingerprint verification failed: %s\n",
                hal_strerror(ret));
        hal_cleanup();
        return -1;
    }

    printf("[vault_manager] Fingerprint verified successfully!\n");
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
    /* 1. Önce doğrudan girişi dene */
    if (is_luks_device(input) == 0) {
        strncpy(output, input, maxlen);
        return;
    }

    /* 2. Bölüm 1'i dene (sdb -> sdb1) */
    char p1[256];
    snprintf(p1, sizeof(p1), "%s1", input);
    if (is_luks_device(p1) == 0) {
        printf("[vault_manager] Found LUKS on partition: %s\n", p1);
        strncpy(output, p1, maxlen);
        return;
    }

    /* 3. Loop cihazı bölümünü dene (loop0 -> loop0p1) */
    snprintf(p1, sizeof(p1), "%sp1", input);
    if (is_luks_device(p1) == 0) {
        printf("[vault_manager] Found LUKS on loop partition: %s\n", p1);
        strncpy(output, p1, maxlen);
        return;
    }

    /* Bulunamadıysa orijinali bırak (hata mesajı cryptsetup'tan gelecek) */
    strncpy(output, input, maxlen);
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
    if (get_biometric_key(finger_id, key) != 0) return 1;

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
        const char *close[] = {"cryptsetup", "close", MAPPER_NAME, NULL};
        run_cmd(close);
        return 1;
    }

    /* 5. Geçici mount → kullanıcı izinlerini ayarla → unmount */
    const char *tmpdir = "/tmp/_vault_setup";
    mkdir(tmpdir, 0755);

    const char *mount_args[] = {"mount", MAPPER_PATH, tmpdir, NULL};
    if (run_cmd(mount_args) == 0) {
        /* SUDO_UID/SUDO_GID ile gerçek kullanıcıyı bul */
        const char *sudo_uid = getenv("SUDO_UID");
        const char *sudo_gid = getenv("SUDO_GID");

        if (sudo_uid && sudo_gid) {
            const char *chown_args[] = {
                "chown", "-R",
                /* uid:gid formatında */
                NULL, tmpdir, NULL
            };
            char ownership[64];
            snprintf(ownership, sizeof(ownership), "%s:%s", sudo_uid, sudo_gid);
            chown_args[2] = ownership;
            run_cmd(chown_args);
        } else {
            /* Root olarak çalıştırılmış, chown gereksiz */
            chmod(tmpdir, 0777);
        }

        const char *umount_args[] = {"umount", tmpdir, NULL};
        run_cmd(umount_args);
    }

    rmdir(tmpdir);

    /* 6. LUKS kapat */
    const char *close_args[] = {"cryptsetup", "close", MAPPER_NAME, NULL};
    run_cmd(close_args);

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
static int cmd_open(const char *device, const char *mount_point, int finger_id)
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
    if (get_biometric_key(finger_id, key) != 0) return 1;

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
 * CLOSE: Unmount → LUKS kapat.
 */
static int cmd_close(const char *mount_point)
{
    printf("[vault_manager] Locking vault...\n");

    /* 1. Önce normal unmount dene */
    printf("[vault_manager] Unmounting %s\n", mount_point);
    const char *umount_args[] = {"umount", mount_point, NULL};

    if (run_cmd(umount_args) != 0) {
        /* 2. Meşgulse, meşgul eden süreçleri öldür (fuser -mk) */
        printf("[vault_manager] Vault is busy. Terminating processes using the mount point...\n");
        const char *fuser_args[] = {"fuser", "-mk", mount_point, NULL};
        run_cmd(fuser_args);

        /* Biraz bekle ve tekrar dene */
        usleep(500000); // 0.5s

        if (run_cmd(umount_args) != 0) {
            fprintf(stderr, "[vault_manager] Error: Could not unmount device even after forcing.\n");
            return 1;
        }
    }

    /* 3. LUKS kapat — kernel'daki key de silinir */
    printf("[vault_manager] Closing LUKS container...\n");
    
    /* 
     * NOT: --deferred KULLANMIYORUZ. 
     * Çünkü kullanıcı kilitlediğinde anahtarın anında RAM'den silinmesini istiyoruz.
     */
    const char *close_args[] = {"cryptsetup", "close", MAPPER_NAME, NULL};

    if (run_cmd(close_args) != 0) {
        fprintf(stderr, "[vault_manager] Warning: LUKS container is busy and could not be closed.\n");
        return 1;
    }

    printf("[vault_manager] ✓ Vault locked and encryption keys wiped from memory.\n");
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
    if (access(MAPPER_PATH, F_OK) == 0) {
        printf("OPEN\n");
        return 0;
    } else {
        printf("CLOSED\n");
        return 1;
    }
}

/* ─── Main ───────────────────────────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "DigitalVault — Vault Manager (LUKS + ext4)\n"
        "\n"
        "Usage:\n"
        "  %s setup     <device> [finger_id]                — First-time LUKS setup\n"
        "  %s open      <device> <mountpoint> [finger_id]   — Unlock vault\n"
        "  %s close     <mountpoint>                        — Lock vault\n"
        "  %s addkey    <device> <owner_fid> <new_fid>      — Grant access\n"
        "  %s killslot  <device> <owner_fid> <slot_num>     — Revoke access\n"
        "  %s listslots <device>                            — List active keyslots\n"
        "  %s status                                        — Check vault status\n"
        "\n",
        prog, prog, prog, prog, prog, prog, prog);
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
        int partition_size = 0;
        const char *vault_name = "DigitalVault";
        const char *normal_name = "Normal";
        const char *normal_fs = "exfat";

        /* Parse optional arguments */
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--force") == 0) {
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

        return cmd_setup(argv[2], fid, force, partition_size, vault_name, normal_name, normal_fs);
    }
    else if (strcmp(cmd, "open") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s open <device> <mountpoint> [finger_id]\n", argv[0]);
            return 1;
        }
        int fid = (argc >= 5) ? atoi(argv[4]) : 1;
        return cmd_open(argv[2], argv[3], fid);
    }
    else if (strcmp(cmd, "close") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s close <mountpoint>\n", argv[0]);
            return 1;
        }
        return cmd_close(argv[2]);
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
    else {
        fprintf(stderr, "[vault_manager] Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
