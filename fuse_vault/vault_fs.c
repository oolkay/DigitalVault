/*
 * Biometric Vault — Main FUSE Filesystem Implementation
 *
 * Bu dosya Linux çekirdeği ile bizim şifreleme motorumuz arasındaki köprüdür.
 * Dosya oluşturma, okuma, yazma, silme, listeleme işlemlerini destekler.
 */

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "hal.h"
#include "block_io.h"
#include "fs_metadata.h"
#include "crypto.h"

/* ─── Global State ───────────────────────────────────────────────────────── */

typedef struct {
    block_io_t        bio;
    char             *image_path;
    int               finger_id;
    vault_superblock_t sb;
    vault_inode_t      inodes[MAX_FILES];
    int                formatted;       /* Filesystem formatlanmış mı? */
    pthread_mutex_t    lock;            /* Thread-safety için mutex */
} vault_context_t;

static vault_context_t g_ctx;

/* ─── Metadata Persistence ───────────────────────────────────────────────── */

static int load_superblock(void) {
    uint8_t buf[SECTOR_SIZE];
    /* Superblock magic/boyut bilgisi — ŞİFRELİ okunur */
    if (bio_read_block(&g_ctx.bio, SUPERBLOCK_BLOCK, buf) != 0) return -1;
    memcpy(&g_ctx.sb, buf, sizeof(vault_superblock_t));
    return 0;
}

static int save_superblock(void) {
    uint8_t buf[SECTOR_SIZE];
    memset(buf, 0, SECTOR_SIZE);
    memcpy(buf, &g_ctx.sb, sizeof(vault_superblock_t));
    /* Superblock ŞİFRELİ yazılır */
    if (bio_write_block(&g_ctx.bio, SUPERBLOCK_BLOCK, buf) != 0) return -1;
    return 0;
}

/**
 * Inode tablosunu diskten oku.
 */
static int load_inodes(void) {
    uint8_t buf[SECTOR_SIZE];
    size_t inode_size = sizeof(vault_inode_t);
    size_t per_block = SECTOR_SIZE / inode_size;

    for (int b = 0; b < INODE_TABLE_BLOCKS; b++) {
        /*
         * Inode tablosu dosya isimlerini içerir — güvenlik için
         * şifreli bio_read_block kullanılır. Key bu noktada yüklü olmalı.
         */
        if (bio_read_block(&g_ctx.bio, INODE_TABLE_START + b, buf) != 0)
            return -1;
        for (size_t i = 0; i < per_block; i++) {
            size_t idx = b * per_block + i;
            if (idx >= MAX_FILES) break;
            memcpy(&g_ctx.inodes[idx], buf + i * inode_size, inode_size);
        }
    }
    return 0;
}

/**
 * Inode tablosunu diske yaz (şifreli).
 */
static int save_inodes(void) {
    uint8_t buf[SECTOR_SIZE];
    size_t inode_size = sizeof(vault_inode_t);
    size_t per_block = SECTOR_SIZE / inode_size;

    for (int b = 0; b < INODE_TABLE_BLOCKS; b++) {
        memset(buf, 0, SECTOR_SIZE);
        for (size_t i = 0; i < per_block; i++) {
            size_t idx = b * per_block + i;
            if (idx >= MAX_FILES) break;
            memcpy(buf + i * inode_size, &g_ctx.inodes[idx], inode_size);
        }
        /* Dosya isimleri hassas veri — AES-256-XTS ile şifrele */
        if (bio_write_block(&g_ctx.bio, INODE_TABLE_START + b, buf) != 0)
            return -1;
    }
    return 0;
}

/**
 * Yeni dosya sistemi formatla (ilk kullanım).
 */
static int format_filesystem(void) {
    printf("[FUSE] Formatting new filesystem...\n");

    if (!g_ctx.bio.key_loaded) {
        fprintf(stderr, "[FUSE] Cannot format: encryption key not loaded\n");
        return -1;
    }

    g_ctx.sb.magic = FS_MAGIC;
    g_ctx.sb.block_size = SECTOR_SIZE;
    g_ctx.sb.total_blocks = g_ctx.bio.total_blocks;
    g_ctx.sb.inode_count = 0;

    memset(g_ctx.inodes, 0, sizeof(g_ctx.inodes));

    if (save_superblock() != 0) return -1;
    /* İlk format: inode bloklarını şifreli sıfır blok olarak yaz */
    if (save_inodes() != 0) return -1;

    g_ctx.formatted = 1;
    printf("[FUSE] Filesystem formatted: %lu blocks available\n",
           g_ctx.sb.total_blocks - DATA_START_BLOCK);
    return 0;
}

/* ─── Inode Helpers ──────────────────────────────────────────────────────── */

/**
 * Dosya adına göre inode bul. Bulamazsa -1 döner.
 * path "/" ile başlar, biz sadece basename'i kullanıyoruz.
 */
static int find_inode(const char *path) {
    const char *name = path + 1; /* "/" karakterini atla */
    for (int i = 0; i < MAX_FILES; i++) {
        if (g_ctx.inodes[i].in_use &&
            strcmp(g_ctx.inodes[i].filename, name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * Boş inode slotu bul.
 */
static int find_free_inode(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!g_ctx.inodes[i].in_use)
            return i;
    }
    return -1;
}

/**
 * Kullanılmayan bir data block bul.
 * Her dosyaya ardışık bloklar atanır, basit first-fit.
 */
static uint64_t allocate_blocks(uint64_t count) {
    /*
     * First-fit allocator: DATA_START_BLOCK'tan itibaren boş alan ara.
     * Her iterasyonda candidate ilerler, bu nedenle sonsuz döngü oluşmaz.
     */
    uint64_t candidate = DATA_START_BLOCK;

    while (candidate + count <= g_ctx.sb.total_blocks) {
        int conflict = 0;
        uint64_t next_candidate = candidate; /* Çakışma durumunda nereye atla */

        for (int i = 0; i < MAX_FILES; i++) {
            if (!g_ctx.inodes[i].in_use) continue;

            uint64_t file_blocks = (g_ctx.inodes[i].size + SECTOR_SIZE - 1) / SECTOR_SIZE;
            if (file_blocks == 0) file_blocks = 1;

            uint64_t fs = g_ctx.inodes[i].start_block;
            uint64_t fe = fs + file_blocks;

            /* [candidate, candidate+count) ile [fs, fe) çakışıyor mu? */
            if (candidate < fe && candidate + count > fs) {
                /* fe noktasından devam et */
                if (fe > next_candidate)
                    next_candidate = fe;
                conflict = 1;
            }
        }

        if (!conflict) {
            return candidate; /* Boş alan bulundu */
        }

        if (next_candidate <= candidate) {
            /* İlerleme olmadı — sonsuz döngüyü kır */
            break;
        }
        candidate = next_candidate;
    }

    return 0; /* Yer bulunamadı */
}

/* ─── FUSE Operations ────────────────────────────────────────────────────── */

/**
 * Dosya/Klasör bilgilerini döndürür (ls komutu için kritiktir).
 */
static int vault_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    pthread_mutex_lock(&g_ctx.lock);
    memset(stbuf, 0, sizeof(struct stat));

    /* Root dizini (/) */
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        pthread_mutex_unlock(&g_ctx.lock);
        return 0;
    }

    /* Alt dizin desteklemiyoruz, sadece /filename */
    if (strchr(path + 1, '/') != NULL) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    int idx = find_inode(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    vault_inode_t *inode = &g_ctx.inodes[idx];
    stbuf->st_mode = S_IFREG | inode->mode;
    stbuf->st_nlink = 1;
    stbuf->st_size = inode->size;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_ctime = inode->ctime;
    stbuf->st_mtime = inode->mtime;
    stbuf->st_atime = inode->mtime;
    stbuf->st_blocks = ((inode->size + 511) / 512);
    stbuf->st_blksize = SECTOR_SIZE;

    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}

/**
 * Klasör içeriğini listeler.
 */
static int vault_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;

    pthread_mutex_lock(&g_ctx.lock);
    if (strcmp(path, "/") != 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* Tüm aktif dosyaları listele */
    for (int i = 0; i < MAX_FILES; i++) {
        if (g_ctx.inodes[i].in_use) {
            filler(buf, g_ctx.inodes[i].filename, NULL, 0, 0);
        }
    }

    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}

/**
 * Dosya oluştur (touch, cp vb. tarafından çağrılır).
 */
static int vault_mknod(const char *path, mode_t mode, dev_t rdev) {
    (void) rdev;
    pthread_mutex_lock(&g_ctx.lock);

    if (strchr(path + 1, '/') != NULL) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -EACCES; /* Alt dizin desteklenmiyor */
    }

    const char *name = path + 1;
    if (strlen(name) >= MAX_FILENAME) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENAMETOOLONG;
    }

    /* Aynı isimde dosya var mı? */
    if (find_inode(path) >= 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -EEXIST;
    }

    /* Boş slot bul */
    int idx = find_free_inode();
    if (idx < 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOSPC;
    }

    /* En az 1 blok ayır */
    uint64_t blk = allocate_blocks(1);
    if (blk == 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOSPC;
    }

    /* Inode'u doldur */
    vault_inode_t *inode = &g_ctx.inodes[idx];
    memset(inode, 0, sizeof(vault_inode_t));
    strncpy(inode->filename, name, MAX_FILENAME - 1);
    inode->size = 0;
    inode->start_block = blk;
    inode->in_use = 1;
    inode->mode = mode & 0777;
    if (inode->mode == 0) inode->mode = 0644;
    inode->ctime = time(NULL);
    inode->mtime = inode->ctime;

    g_ctx.sb.inode_count++;

    /* Bloku sıfırla */
    uint8_t zero[SECTOR_SIZE];
    memset(zero, 0, SECTOR_SIZE);
    bio_write_block(&g_ctx.bio, blk, zero);

    /* Metadata'yı diske kaydet */
    save_superblock();
    save_inodes();

    printf("[FUSE] Created file: %s (inode %d, block %lu)\n", name, idx, blk);
    printf("[AUDIT] CREATE: %s\n", name);
    fflush(stdout);
    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}

/**
 * Dosya oluştur (create — mknod + open birleşimi).
 */
static int vault_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int ret = vault_mknod(path, mode, 0);
    if (ret != 0) return ret;
    fi->fh = find_inode(path);
    return 0;
}

/**
 * Dosya aç.
 */
static int vault_open(const char *path, struct fuse_file_info *fi) {
    int idx = find_inode(path);
    if (idx < 0)
        return -ENOENT;
    fi->fh = idx;
    return 0;
}

/**
 * Dosyadan oku — şifre çözme otomatik yapılır.
 */
static int vault_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    (void) path;
    pthread_mutex_lock(&g_ctx.lock);
    int idx = fi ? (int)fi->fh : find_inode(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    vault_inode_t *inode = &g_ctx.inodes[idx];

    /* Offset dosya sonunu aşıyorsa */
    if ((uint64_t)offset >= inode->size) {
        pthread_mutex_unlock(&g_ctx.lock);
        return 0;
    }

    /* Okunabilecek miktar */
    if (offset + size > inode->size)
        size = inode->size - offset;

    size_t bytes_read = 0;
    uint8_t block_buf[SECTOR_SIZE];

    while (bytes_read < size) {
        uint64_t file_block = (offset + bytes_read) / SECTOR_SIZE;
        size_t   block_off  = (offset + bytes_read) % SECTOR_SIZE;
        size_t   to_copy    = SECTOR_SIZE - block_off;

        if (to_copy > size - bytes_read)
            to_copy = size - bytes_read;

        uint64_t disk_block = inode->start_block + file_block;

        if (bio_read_block(&g_ctx.bio, disk_block, block_buf) != 0) {
            pthread_mutex_unlock(&g_ctx.lock);
            return -EIO;
        }

        memcpy(buf + bytes_read, block_buf + block_off, to_copy);
        bytes_read += to_copy;
    }

    pthread_mutex_unlock(&g_ctx.lock);
    return (int)bytes_read;
}

/**
 * Dosyaya yaz — şifreleme otomatik yapılır.
 */
static int vault_write(const char *path, const char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) path;
    pthread_mutex_lock(&g_ctx.lock);
    int idx = fi ? (int)fi->fh : find_inode(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    vault_inode_t *inode = &g_ctx.inodes[idx];

    /* Yazma sonrası yeni boyut */
    uint64_t new_end = offset + size;

    /* Gerekli blok sayısı */
    uint64_t needed_blocks = (new_end + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint64_t current_blocks = (inode->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    if (current_blocks == 0) current_blocks = 1;

    /* Daha fazla blok gerekiyorsa yeniden allocate et */
    if (needed_blocks > current_blocks) {
        /* Mevcut bloklardan devam edebilecek mi kontrol et */
        uint64_t extra = needed_blocks - current_blocks;
        uint64_t next_block = inode->start_block + current_blocks;

        /* Basit kontrol: sonraki bloklar boş mu? */
        int can_extend = 1;
        for (int i = 0; i < MAX_FILES; i++) {
            if (!g_ctx.inodes[i].in_use || i == idx) continue;
            uint64_t fs = g_ctx.inodes[i].start_block;
            uint64_t fb = (g_ctx.inodes[i].size + SECTOR_SIZE - 1) / SECTOR_SIZE;
            if (fb == 0) fb = 1;
            uint64_t fe = fs + fb;
            if (next_block + extra > fs && next_block < fe) {
                can_extend = 0;
                break;
            }
        }

        if (!can_extend || next_block + extra > g_ctx.sb.total_blocks) {
            pthread_mutex_unlock(&g_ctx.lock);
            return -ENOSPC;
        }

        /* Yeni blokları sıfırla */
        uint8_t zero[SECTOR_SIZE];
        memset(zero, 0, SECTOR_SIZE);
        for (uint64_t b = current_blocks; b < needed_blocks; b++) {
            bio_write_block(&g_ctx.bio, inode->start_block + b, zero);
        }
    }

    /* Veriyi yaz */
    size_t bytes_written = 0;
    uint8_t block_buf[SECTOR_SIZE];

    while (bytes_written < size) {
        uint64_t file_block = (offset + bytes_written) / SECTOR_SIZE;
        size_t   block_off  = (offset + bytes_written) % SECTOR_SIZE;
        size_t   to_copy    = SECTOR_SIZE - block_off;

        if (to_copy > size - bytes_written)
            to_copy = size - bytes_written;

        uint64_t disk_block = inode->start_block + file_block;

        /* Partial write ise önce mevcut bloğu oku */
        if (block_off != 0 || to_copy != SECTOR_SIZE) {
            if (bio_read_block(&g_ctx.bio, disk_block, block_buf) != 0)
                memset(block_buf, 0, SECTOR_SIZE); /* İlk yazma */
        }

        memcpy(block_buf + block_off, buf + bytes_written, to_copy);

        if (bio_write_block(&g_ctx.bio, disk_block, block_buf) != 0) {
            pthread_mutex_unlock(&g_ctx.lock);
            return -EIO;
        }

        bytes_written += to_copy;
    }

    /* Dosya boyutunu güncelle */
    if (new_end > inode->size)
        inode->size = new_end;
    inode->mtime = time(NULL);

    /* Metadata kaydet */
    save_superblock();
    save_inodes();

    printf("[AUDIT] WRITE: %s (%zu bytes)\n", inode->filename, size);
    fflush(stdout);

    pthread_mutex_unlock(&g_ctx.lock);
    return (int)bytes_written;
}

/**
 * Dosya sil.
 */
static int vault_unlink(const char *path) {
    pthread_mutex_lock(&g_ctx.lock);
    int idx = find_inode(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    vault_inode_t *inode = &g_ctx.inodes[idx];

    /* Dosyanın bloklarını sıfırla (güvenlik için) */
    uint64_t blocks = (inode->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    if (blocks == 0) blocks = 1;
    uint8_t zero[SECTOR_SIZE];
    memset(zero, 0, SECTOR_SIZE);
    for (uint64_t b = 0; b < blocks; b++) {
        bio_write_block(&g_ctx.bio, inode->start_block + b, zero);
    }

    printf("[FUSE] Deleted file: %s (inode %d)\n", inode->filename, idx);
    printf("[AUDIT] DELETE: %s\n", inode->filename);
    fflush(stdout);

    /* Inode'u temizle */
    memset(inode, 0, sizeof(vault_inode_t));
    g_ctx.sb.inode_count--;

    save_superblock();
    save_inodes();

    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}

/**
 * Dosya boyutunu değiştir.
 */
static int vault_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void) fi;
    pthread_mutex_lock(&g_ctx.lock);
    int idx = find_inode(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    vault_inode_t *inode = &g_ctx.inodes[idx];

    if ((uint64_t)size > inode->size) {
        /* Büyütme — yeni bloklar gerekebilir */
        uint64_t needed = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
        uint64_t current = (inode->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
        if (current == 0) current = 1;

        if (needed > current) {
            uint64_t next = inode->start_block + current;
            if (next + (needed - current) > g_ctx.sb.total_blocks) {
                pthread_mutex_unlock(&g_ctx.lock);
                return -ENOSPC;
            }

            uint8_t zero[SECTOR_SIZE];
            memset(zero, 0, SECTOR_SIZE);
            for (uint64_t b = current; b < needed; b++)
                bio_write_block(&g_ctx.bio, inode->start_block + b, zero);
        }
    }

    inode->size = size;
    inode->mtime = time(NULL);

    save_superblock();
    save_inodes();
    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}

/**
 * Zaman bilgisini güncelle.
 */
static int vault_utimens(const char *path, const struct timespec tv[2],
                         struct fuse_file_info *fi) {
    (void) fi;
    pthread_mutex_lock(&g_ctx.lock);
    int idx = find_inode(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    vault_inode_t *inode = &g_ctx.inodes[idx];
    if (tv) {
        inode->mtime = tv[1].tv_sec;
    } else {
        inode->mtime = time(NULL);
    }

    save_inodes();
    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}

/**
 * Dosya adını değiştir.
 */
static int vault_rename(const char *from, const char *to, unsigned int flags) {
    (void) flags;
    pthread_mutex_lock(&g_ctx.lock);

    int idx = find_inode(from);
    if (idx < 0) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENOENT;
    }

    /* Hedef isimde dosya varsa onu sil (Overwrite davranışı) */
    int target_idx = find_inode(to);
    if (target_idx >= 0) {
        memset(&g_ctx.inodes[target_idx], 0, sizeof(vault_inode_t));
        g_ctx.sb.inode_count--;
    }

    const char *new_name = to + 1;
    if (strlen(new_name) >= MAX_FILENAME) {
        pthread_mutex_unlock(&g_ctx.lock);
        return -ENAMETOOLONG;
    }

    strncpy(g_ctx.inodes[idx].filename, new_name, MAX_FILENAME - 1);
    g_ctx.inodes[idx].mtime = time(NULL);

    save_inodes();
    printf("[AUDIT] RENAME: %s -> %s\n", from, to);
    fflush(stdout);
    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}

/**
 * Dosya sistemi istatistiklerini döndür (df komutu).
 */
static int vault_statfs(const char *path, struct statvfs *stbuf) {
    (void) path;
    pthread_mutex_lock(&g_ctx.lock);

    stbuf->f_bsize  = SECTOR_SIZE;
    stbuf->f_frsize = SECTOR_SIZE;
    stbuf->f_blocks = g_ctx.sb.total_blocks;
    stbuf->f_bfree  = g_ctx.sb.total_blocks - DATA_START_BLOCK; /* Basitleştirilmiş */
    stbuf->f_bavail = stbuf->f_bfree;
    stbuf->f_files  = MAX_FILES;
    stbuf->f_ffree  = MAX_FILES - g_ctx.sb.inode_count;
    stbuf->f_namemax = MAX_FILENAME;

    pthread_mutex_unlock(&g_ctx.lock);
    return 0;
}

/**
 * Dosya sistemi ilk açıldığında çalışır.
 * Burada parmak izi doğrulaması yapıp anahtarı yükleyeceğiz.
 */
static void *vault_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void) conn;
    cfg->kernel_cache = 0; // Şifreli veride cache tehlikeli olabilir

    printf("[FUSE] Initializing Vault FS...\n");

    /* Mutex başlat */
    pthread_mutex_init(&g_ctx.lock, NULL);

    /* 1. HAL Başlat */
    if (hal_init("simulator:/tmp/biometric_vault.sock") != HAL_OK) {
        fprintf(stderr, "[FUSE] Failed to connect to hardware!\n");
        exit(1);
    }

    /* 2. Parmak izi doğrula */
    uint8_t master_key[MASTER_KEY_SIZE];
    printf("[FUSE] Please verify fingerprint on sensor...\n");

    if (hal_verify_finger(g_ctx.finger_id, master_key) != HAL_OK) {
        fprintf(stderr, "[FUSE] Fingerprint verification FAILED!\n");
        crypto_secure_wipe(master_key, MASTER_KEY_SIZE);
        exit(1);
    }

    printf("[FUSE] Fingerprint verified successfully!\n");
    printf("[AUDIT] UNLOCK: Fingerprint ID %d\n", g_ctx.finger_id);
    fflush(stdout);

    /* 3. Anahtarı Block I/O katmanına yükle */
    if (bio_load_key(&g_ctx.bio, master_key) != 0) {
        fprintf(stderr, "[FUSE] Failed to load encryption key!\n");
        crypto_secure_wipe(master_key, MASTER_KEY_SIZE);
        exit(1);
    }

    /* Master key'i RAM'den temizle (artık XTS key türetildi) */
    crypto_secure_wipe(master_key, MASTER_KEY_SIZE);

    /* 4. Superblock kontrolü — format veya yükle */
    if (load_superblock() == 0 && g_ctx.sb.magic == FS_MAGIC) {
        printf("[FUSE] Existing filesystem detected (%u files).\n", g_ctx.sb.inode_count);
        if (load_inodes() != 0) {
            fprintf(stderr, "[FUSE] Failed to load inode table!\n");
            exit(1);
        }
        g_ctx.formatted = 1;
    } else {
        printf("[FUSE] No filesystem found, formatting...\n");
        if (format_filesystem() != 0) {
            fprintf(stderr, "[FUSE] Format failed!\n");
            exit(1);
        }
    }

    printf("[FUSE] Vault ready. %u files, %lu total blocks.\n",
           g_ctx.sb.inode_count, g_ctx.sb.total_blocks);

    return &g_ctx;
}

/**
 * Dosya sistemi kapatıldığında çalışır.
 */
static void vault_destroy(void *private_data) {
    (void) private_data;
    printf("[FUSE] Shutting down vault...\n");

    /* Metadata'yı son kez kaydet */
    save_superblock();
    save_inodes();

    /* Block I/O kapat (key'ler silinir) */
    bio_close(&g_ctx.bio);

    /* Inode tablosunu bellekten temizle */
    crypto_secure_wipe(g_ctx.inodes, sizeof(g_ctx.inodes));

    pthread_mutex_destroy(&g_ctx.lock);

    hal_cleanup();
    printf("[FUSE] Vault shut down safely.\n");
}

/* FUSE operasyon tablosu */
static const struct fuse_operations vault_ops = {
    .init       = vault_init,
    .destroy    = vault_destroy,
    .getattr    = vault_getattr,
    .readdir    = vault_readdir,
    .mknod      = vault_mknod,
    .create     = vault_create,
    .open       = vault_open,
    .read       = vault_read,
    .write      = vault_write,
    .unlink     = vault_unlink,
    .truncate   = vault_truncate,
    .utimens    = vault_utimens,
    .rename     = vault_rename,
    .statfs     = vault_statfs,
};

/* ─── Main ───────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    /* Varsayılan ayarlar */
    g_ctx.image_path = "vault.img";
    g_ctx.finger_id = 1;
    g_ctx.formatted = 0;

    /* 
     * Basit argüman kontrolü:
     * Eğer 2'den fazla argüman varsa ve sonuncusu bir klasör değilse 
     * onu image_path olarak kabul et. (Daha profesyonel bir parser eklenebilir)
     */
    if (argc >= 3) {
        /* Son argüman mountpoint olacak, ondan öncekini image_path olarak alalım */
        /* fuse_main argümanları temizleyeceği için biz şimdilik kopyasını alıyoruz */
        g_ctx.image_path = argv[argc - 1];
        argc--; /* Mountpoint'i fuse_main'e bırak, image_path'i biz aldık */
    }

    printf("[FUSE] Target Vault: %s\n", g_ctx.image_path);

    /* Image dosyası/cihazı yoksa ve bir dosya yoluysa oluştur (16MB varsayılan) */
    if (access(g_ctx.image_path, F_OK) != 0) {
        /* Cihaz dosyaları (/dev/sd...) genelde zaten vardır. 
         * Yoksa bu bir dosya yoludur, oluştur. */
        if (strstr(g_ctx.image_path, "/dev/") == NULL) {
            printf("[FUSE] Creating new vault image: %s (16 MB)\n", g_ctx.image_path);
            if (bio_create_image(g_ctx.image_path, 16) != 0) {
                fprintf(stderr, "[FUSE] Failed to create vault image!\n");
                return 1;
            }
        } else {
            fprintf(stderr, "[FUSE] Error: Device %s not found!\n", g_ctx.image_path);
            return 1;
        }
    }

    /* Image dosyasını aç */
    if (bio_open(&g_ctx.bio, g_ctx.image_path) != 0) {
        fprintf(stderr, "[FUSE] Failed to open %s\n", g_ctx.image_path);
        return 1;
    }

    printf("Starting Biometric Vault FS on mount point: %s\n", argv[argc-1]);
    return fuse_main(argc, argv, &vault_ops, NULL);
}
