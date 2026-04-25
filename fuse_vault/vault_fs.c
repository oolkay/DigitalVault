/*
 * Biometric Vault — Main FUSE Filesystem Implementation
 *
 * Bu dosya Linux çekirdeği ile bizim şifreleme motorumuz arasındaki köprüdür.
 */

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "hal.h"
#include "block_io.h"
#include "fs_metadata.h"

/* ─── Global State ───────────────────────────────────────────────────────── */

typedef struct {
    block_io_t bio;
    char *image_path;
    int   finger_id;
} vault_context_t;

static vault_context_t g_ctx;

/* ─── Helper Functions ───────────────────────────────────────────────────── */

/**
 * Superblock'u oku.
 */
static int load_superblock(vault_superblock_t *sb) {
    uint8_t buf[SECTOR_SIZE];
    if (bio_read_raw(&g_ctx.bio, SUPERBLOCK_BLOCK, buf) != 0) return -1;
    memcpy(sb, buf, sizeof(vault_superblock_t));
    return 0;
}

/* ─── FUSE Operations ────────────────────────────────────────────────────── */

/**
 * Dosya/Klasör bilgilerini döndürür (ls komutu için kritiktir).
 */
static int vault_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    /* Root dizini (/) */
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    /* Şimdilik sadece root dizini gösterelim, dosyaları sonraki adımda ekleyeceğiz */
    return -ENOENT;
}

/**
 * Klasör içeriğini listeler.
 */
static int vault_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;

    if (strcmp(path, "/") != 0) return -ENOENT;

    /* Standart klasörler */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* Dosya listeleme mantığı buraya gelecek */

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

    /* 1. HAL Başlat */
    if (hal_init("simulator:/tmp/biometric_vault.sock") != HAL_OK) {
        fprintf(stderr, "[FUSE] Failed to connect to hardware!\n");
        exit(1);
    }

    /* 2. Parmak izi doğrula (Hardcoded finger_id: 1) */
    uint8_t master_key[MASTER_KEY_SIZE];
    printf("[FUSE] Please verify fingerprint on sensor...\n");
    
    if (hal_verify_finger(g_ctx.finger_id, master_key) != HAL_OK) {
        fprintf(stderr, "[FUSE] Fingerprint verification FAILED!\n");
        exit(1);
    }

    /* 3. Anahtarı Block I/O katmanına yükle */
    if (bio_load_key(&g_ctx.bio, master_key) != 0) {
        fprintf(stderr, "[FUSE] Failed to load encryption key!\n");
        exit(1);
    }

    /* 4. Superblock kontrolü */
    vault_superblock_t sb;
    if (load_superblock(&sb) == 0 && sb.magic == FS_MAGIC) {
        printf("[FUSE] Existing filesystem detected.\n");
    } else {
        printf("[FUSE] No filesystem found, new one will be created upon use.\n");
    }

    return &g_ctx;
}

/**
 * Dosya sistemi kapatıldığında çalışır.
 */
static void vault_destroy(void *private_data) {
    (void) private_data;
    printf("[FUSE] Shutting down vault...\n");
    bio_close(&g_ctx.bio);
    hal_cleanup();
}

/* FUSE operasyon tablosu */
static const struct fuse_operations vault_ops = {
    .init       = vault_init,
    .destroy    = vault_destroy,
    .getattr    = vault_getattr,
    .readdir    = vault_readdir,
    /* read, write, create vb. fonksiyonlar bir sonraki adımda eklenecek */
};

/* ─── Main ───────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    /* Varsayılan ayarlar */
    g_ctx.image_path = "vault.img";
    g_ctx.finger_id = 1;

    /* Image dosyasını aç (yoksa hata ver) */
    if (bio_open(&g_ctx.bio, g_ctx.image_path) != 0) {
        fprintf(stderr, "Please create %s first using 'dd' or a setup tool.\n", g_ctx.image_path);
        return 1;
    }

    printf("Starting Biometric Vault FS on mount point...\n");
    return fuse_main(argc, argv, &vault_ops, NULL);
}
