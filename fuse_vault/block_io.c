/*
 * Biometric Vault — Block I/O Implementation
 *
 * Ham disk dosyası üzerinden şifreli okuma/yazma işlemlerini yürütür.
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include "block_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

/* ─── Image Creation ─────────────────────────────────────────────────────── */

int bio_create_image(const char *image_path, uint32_t size_mb)
{
    /* 
     * Belirtilen boyutta boş (sparse) bir dosya oluşturur. 
     * Bu dosya bizim şifreli diskimiz olacak.
     */
    int fd = open(image_path, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        if (errno == EEXIST) {
            fprintf(stderr, "[BIO] Image already exists: %s\n", image_path);
            return 0; // Zaten varsa hata verme
        }
        perror("[BIO] Failed to create image");
        return -1;
    }

    uint64_t size_bytes = (uint64_t)size_mb * 1024 * 1024;
    
    /* Dosya boyutunu ayarla (ftruncate) */
    if (ftruncate(fd, size_bytes) < 0) {
        perror("[BIO] Failed to set image size");
        close(fd);
        return -1;
    }

    close(fd);
    printf("[BIO] Created new %u MB image: %s\n", size_mb, image_path);
    return 0;
}

/* ─── Open / Close ───────────────────────────────────────────────────────── */

int bio_open(block_io_t *bio, const char *image_path)
{
    memset(bio, 0, sizeof(block_io_t));

    bio->fd = open(image_path, O_RDWR);
    if (bio->fd < 0) {
        perror("[BIO] Failed to open image");
        return -1;
    }
    /* Dosya boyutunu öğren */
    struct stat st;
    if (fstat(bio->fd, &st) < 0) {
        perror("[BIO] Failed to fstat image");
        close(bio->fd);
        return -1;
    }

    bio->image_size = st.st_size;
    bio->total_blocks = st.st_size / SECTOR_SIZE;

    printf("[BIO] Opened image: %s (%lu sectors, %lu MB)\n", 
           image_path, bio->total_blocks, bio->image_size / (1024*1024));

    return 0;
}

void bio_close(block_io_t *bio)
{
    if (bio->fd >= 0) {
        close(bio->fd);
        bio->fd = -1;
    }
    /* Key'i bellekten güvenli şekilde temizle */
    crypto_secure_wipe(bio->xts_key, XTS_KEY_SIZE);
    bio->key_loaded = 0;
    printf("[BIO] Image closed and keys wiped from memory\n");
}

/* ─── Key Loading ────────────────────────────────────────────────────────── */

int bio_load_key(block_io_t *bio, const uint8_t *master_key)
{
    /* HAL'dan gelen 32-byte anahtardan, 64-byte XTS anahtarı türet */
    if (crypto_derive_xts_key(master_key, bio->xts_key) != 0) {
        return -1;
    }
    bio->key_loaded = 1;
    printf("[BIO] Encryption key successfully derived and loaded\n");
    return 0;
}

/* ─── Block Read/Write ───────────────────────────────────────────────────── */

int bio_read_block(block_io_t *bio, uint64_t block_num, uint8_t *buffer)
{
    if (!bio->key_loaded) {
        fprintf(stderr, "[BIO] Error: Attempted read without key\n");
        return -1;
    }

    if (block_num >= bio->total_blocks) {
        fprintf(stderr, "[BIO] Error: Out of bounds read (block %lu)\n", block_num);
        return -1;
    }

    /* Şifreli veriyi geçici buffer'a oku */
    uint8_t encrypted[SECTOR_SIZE];
    off_t offset = block_num * SECTOR_SIZE;

    if (pread(bio->fd, encrypted, SECTOR_SIZE, offset) != SECTOR_SIZE) {
        perror("[BIO] pread failed");
        return -1;
    }

    /* Veriyi çöz */
    if (crypto_decrypt_sector(encrypted, buffer, block_num, bio->xts_key) != 0) {
        return -1;
    }

    return 0;
}

int bio_write_block(block_io_t *bio, uint64_t block_num, const uint8_t *buffer)
{
    if (!bio->key_loaded) {
        fprintf(stderr, "[BIO] Error: Attempted write without key\n");
        return -1;
    }

    if (block_num >= bio->total_blocks) {
        fprintf(stderr, "[BIO] Error: Out of bounds write (block %lu)\n", block_num);
        return -1;
    }

    /* Veriyi şifrele */
    uint8_t encrypted[SECTOR_SIZE];
    if (crypto_encrypt_sector(buffer, encrypted, block_num, bio->xts_key) != 0) {
        return -1;
    }

    /* Şifreli veriyi diske yaz */
    off_t offset = block_num * SECTOR_SIZE;
    if (pwrite(bio->fd, encrypted, SECTOR_SIZE, offset) != SECTOR_SIZE) {
        perror("[BIO] pwrite failed");
        return -1;
    }

    return 0;
}

/* ─── Raw Read/Write (Unencrypted) ───────────────────────────────────────── */

int bio_read_raw(block_io_t *bio, uint64_t block_num, uint8_t *buffer)
{
    off_t offset = block_num * SECTOR_SIZE;
    if (pread(bio->fd, buffer, SECTOR_SIZE, offset) != SECTOR_SIZE) {
        perror("[BIO] pread raw failed");
        return -1;
    }
    return 0;
}

int bio_write_raw(block_io_t *bio, uint64_t block_num, const uint8_t *buffer)
{
    off_t offset = block_num * SECTOR_SIZE;
    if (pwrite(bio->fd, buffer, SECTOR_SIZE, offset) != SECTOR_SIZE) {
        perror("[BIO] pwrite raw failed");
        return -1;
    }
    return 0;
}
