/*
 * Biometric Vault — Block I/O Layer
 *
 * Ham blok seviyesinde okuma/yazma ve şifreli I/O.
 * Backing store olarak bir image dosyası veya /dev/sdX kullanılır.
 *
 * Her blok SECTOR_SIZE (4KB) büyüklüğündedir.
 * Yazma sırasında otomatik şifrelenir, okuma sırasında çözülür.
 */

#ifndef BLOCK_IO_H
#define BLOCK_IO_H

#include <stdint.h>
#include <stddef.h>
#include "crypto.h"

/**
 * Block I/O context — backing store bağlantısı.
 */
typedef struct {
    int       fd;               /* Dosya descriptor */
    uint64_t  total_blocks;     /* Toplam blok sayısı */
    uint64_t  image_size;       /* Image boyutu (byte) */
    uint8_t   xts_key[XTS_KEY_SIZE]; /* Türetilmiş XTS key */
    int       key_loaded;       /* Key yüklü mü? */
} block_io_t;

/**
 * Backing store'u aç (image dosyası veya block device).
 *
 * @param bio          Context (çağıran tarafından ayrılmış)
 * @param image_path   Image dosyası yolu
 * @return 0 başarılı, -1 hata
 */
int bio_open(block_io_t *bio, const char *image_path);

/**
 * Şifreleme anahtarını yükle (HAL'dan gelen master key).
 *
 * @param bio         Context
 * @param master_key  32 byte AES-256 master key
 * @return 0 başarılı, -1 hata
 */
int bio_load_key(block_io_t *bio, const uint8_t *master_key);

/**
 * Yeni bir boş image dosyası oluştur.
 *
 * @param image_path   Oluşturulacak dosya yolu
 * @param size_mb      Boyut (MB cinsinden)
 * @return 0 başarılı, -1 hata
 */
int bio_create_image(const char *image_path, uint32_t size_mb);

/**
 * Bir bloğu oku ve şifresini çöz.
 *
 * @param bio         Context
 * @param block_num   Blok numarası
 * @param buffer      Çıktı buffer'ı (SECTOR_SIZE byte)
 * @return 0 başarılı, -1 hata
 */
int bio_read_block(block_io_t *bio, uint64_t block_num, uint8_t *buffer);

/**
 * Bir bloğu şifrele ve yaz.
 *
 * @param bio         Context
 * @param block_num   Blok numarası
 * @param buffer      Yazılacak veri (SECTOR_SIZE byte)
 * @return 0 başarılı, -1 hata
 */
int bio_write_block(block_io_t *bio, uint64_t block_num, const uint8_t *buffer);

/**
 * Bir bloğu şifreleme YAPMADAN (raw) oku.
 * Superblock gibi şifrelenmemiş metadata blokları için.
 *
 * @param bio         Context
 * @param block_num   Blok numarası
 * @param buffer      Çıktı buffer'ı (SECTOR_SIZE byte)
 * @return 0 başarılı, -1 hata
 */
int bio_read_raw(block_io_t *bio, uint64_t block_num, uint8_t *buffer);

/**
 * Bir bloğu şifreleme YAPMADAN (raw) yaz.
 *
 * @param bio         Context
 * @param block_num   Blok numarası
 * @param buffer      Yazılacak veri (SECTOR_SIZE byte)
 * @return 0 başarılı, -1 hata
 */
int bio_write_raw(block_io_t *bio, uint64_t block_num, const uint8_t *buffer);

/**
 * Backing store'u kapat ve kaynakları temizle.
 *
 * @param bio  Context
 */
void bio_close(block_io_t *bio);

#endif /* BLOCK_IO_H */
