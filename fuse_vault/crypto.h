/*
 * Biometric Vault — Sector-Level Encryption (AES-256-XTS)
 *
 * USB partition üzerindeki her 4KB bloğu (sektörü) şifreler/çözer.
 * XTS modu disk encryption standardıdır (IEEE P1619).
 *
 * XTS nasıl çalışır:
 *   - İki adet AES-256 key kullanır (toplam 64 byte)
 *   - Her sektörün numarası "tweak" olarak kullanılır
 *   - Aynı veri farklı sektörlerde farklı ciphertext üretir
 *   - Bu sayede pattern analizi saldırıları engellenir
 *
 * Key derivation:
 *   HAL'dan 32 byte master key gelir.
 *   HKDF ile bu key'den 64 byte XTS key türetilir.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

/* Sektör boyutu (4KB) */
#define SECTOR_SIZE     4096

/* AES-256-XTS toplam key boyutu (2 × 32 byte) */
#define XTS_KEY_SIZE    64

/* HAL'dan gelen master key boyutu */
#define MASTER_KEY_SIZE 32

/**
 * Master key'den XTS key türet (HKDF kullanarak).
 *
 * @param master_key    32 byte HAL'dan gelen key
 * @param xts_key_out   64 byte XTS key (çağıran tarafından ayrılmış)
 * @return 0 başarılı, -1 hata
 */
int crypto_derive_xts_key(const uint8_t *master_key, uint8_t *xts_key_out);

/**
 * Tek bir sektörü şifrele.
 *
 * @param plaintext     Şifrelenmemiş veri (SECTOR_SIZE byte)
 * @param ciphertext    Şifrelenmiş veri çıktısı (SECTOR_SIZE byte)
 * @param sector_num    Sektör numarası (XTS tweak olarak kullanılır)
 * @param xts_key       64 byte XTS key
 * @return 0 başarılı, -1 hata
 */
int crypto_encrypt_sector(const uint8_t *plaintext, uint8_t *ciphertext,
                          uint64_t sector_num, const uint8_t *xts_key);

/**
 * Tek bir sektörü çöz.
 *
 * @param ciphertext    Şifrelenmiş veri (SECTOR_SIZE byte)
 * @param plaintext     Çözülmüş veri çıktısı (SECTOR_SIZE byte)
 * @param sector_num    Sektör numarası (XTS tweak olarak kullanılır)
 * @param xts_key       64 byte XTS key
 * @return 0 başarılı, -1 hata
 */
int crypto_decrypt_sector(const uint8_t *ciphertext, uint8_t *plaintext,
                          uint64_t sector_num, const uint8_t *xts_key);

/**
 * Bellekteki key verisini güvenli şekilde sil.
 * Key'in RAM'de kalıntı bırakmaması için kullanılır.
 *
 * @param buf   Silinecek buffer
 * @param len   Buffer uzunluğu
 */
void crypto_secure_wipe(void *buf, size_t len);

#endif /* CRYPTO_H */
