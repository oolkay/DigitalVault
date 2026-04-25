/*
 * Biometric Vault — Sector-Level Encryption Implementation
 *
 * OpenSSL EVP API kullanarak AES-256-XTS şifreleme.
 */

#include "crypto.h"

#include <string.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/err.h>

/* ─── Key Derivation ─────────────────────────────────────────────────────── */

int crypto_derive_xts_key(const uint8_t *master_key, uint8_t *xts_key_out)
{
    /*
     * HKDF (HMAC-based Key Derivation Function) ile
     * 32 byte master key → 64 byte XTS key türetiyoruz.
     *
     * Bu güvenli bir yaklaşımdır çünkü:
     * - Master key'i doğrudan kullanmıyoruz
     * - XTS'in gerektirdiği iki ayrı key'i tek kaynaktan türetiyoruz
     * - Salt ve info parametreleri ile domain separation sağlıyoruz
     */

    EVP_PKEY_CTX *ctx = NULL;
    size_t outlen = XTS_KEY_SIZE;
    int ret = -1;

    /* Sabit salt ve info (uygulama-spesifik) */
    const uint8_t salt[] = "BiometricVault-XTS-Salt-v1";
    const uint8_t info[] = "BiometricVault-XTS-Key-Derivation";

    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (!ctx) goto err;

    if (EVP_PKEY_derive_init(ctx) <= 0) goto err;
    if (EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) <= 0) goto err;
    if (EVP_PKEY_CTX_set1_hkdf_salt(ctx, salt, sizeof(salt) - 1) <= 0) goto err;
    if (EVP_PKEY_CTX_set1_hkdf_key(ctx, master_key, MASTER_KEY_SIZE) <= 0) goto err;
    if (EVP_PKEY_CTX_add1_hkdf_info(ctx, info, sizeof(info) - 1) <= 0) goto err;

    if (EVP_PKEY_derive(ctx, xts_key_out, &outlen) <= 0) goto err;

    ret = 0;

err:
    if (ctx) EVP_PKEY_CTX_free(ctx);
    if (ret != 0) {
        fprintf(stderr, "[CRYPTO] HKDF key derivation failed\n");
        ERR_print_errors_fp(stderr);
    }
    return ret;
}

/* ─── Sector Encryption ──────────────────────────────────────────────────── */

int crypto_encrypt_sector(const uint8_t *plaintext, uint8_t *ciphertext,
                          uint64_t sector_num, const uint8_t *xts_key)
{
    EVP_CIPHER_CTX *ctx = NULL;
    int len, ret = -1;

    /*
     * XTS tweak: 16 byte — sektör numarasını little-endian olarak yazıyoruz.
     * Bu sayede her sektör farklı bir tweak ile şifrelenir.
     */
    uint8_t tweak[16];
    memset(tweak, 0, sizeof(tweak));
    memcpy(tweak, &sector_num, sizeof(sector_num));

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) goto err;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_xts(), NULL, xts_key, tweak) != 1)
        goto err;

    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, SECTOR_SIZE) != 1)
        goto err;

    int total = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext + total, &len) != 1)
        goto err;

    ret = 0;

err:
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    if (ret != 0) {
        fprintf(stderr, "[CRYPTO] Encrypt sector %lu failed\n", sector_num);
        ERR_print_errors_fp(stderr);
    }
    return ret;
}

/* ─── Sector Decryption ──────────────────────────────────────────────────── */

int crypto_decrypt_sector(const uint8_t *ciphertext, uint8_t *plaintext,
                          uint64_t sector_num, const uint8_t *xts_key)
{
    EVP_CIPHER_CTX *ctx = NULL;
    int len, ret = -1;

    uint8_t tweak[16];
    memset(tweak, 0, sizeof(tweak));
    memcpy(tweak, &sector_num, sizeof(sector_num));

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) goto err;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_xts(), NULL, xts_key, tweak) != 1)
        goto err;

    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, SECTOR_SIZE) != 1)
        goto err;

    int total = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext + total, &len) != 1)
        goto err;

    ret = 0;

err:
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    if (ret != 0) {
        fprintf(stderr, "[CRYPTO] Decrypt sector %lu failed\n", sector_num);
        ERR_print_errors_fp(stderr);
    }
    return ret;
}

/* ─── Secure Wipe ────────────────────────────────────────────────────────── */

void crypto_secure_wipe(void *buf, size_t len)
{
    /*
     * OPENSSL_cleanse, compiler'ın "bu memset gereksiz"
     * deyip optimize etmesini (silmesini) engeller.
     * Güvenlik açısından kritiktir.
     */
    OPENSSL_cleanse(buf, len);
}
