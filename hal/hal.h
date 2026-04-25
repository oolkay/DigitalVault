/*
 * Biometric Vault — Hardware Abstraction Layer (HAL)
 *
 * FUSE filesystem ve Vault Manager bu header'ı kullanarak
 * donanımla iletişim kurar. Arka taraftaki implementasyon
 * simulator veya gerçek serial olabilir — bu katman
 * aradaki farkı gizler.
 *
 * Kullanım:
 *   hal_init("simulator:/tmp/biometric_vault.sock")  → Simülatöre bağlan
 *   hal_init("serial:/dev/ttyUSB0")                  → Gerçek donanıma bağlan (sonra)
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ──────────────────────────────────────────────────────────── */

#define HAL_KEY_SIZE        32      /* AES-256 key boyutu (byte) */
#define HAL_MAX_FINGERS     127     /* AS608 maksimum parmak izi kapasitesi */
#define HAL_MAX_NAME_LEN    64      /* Parmak izi adı maksimum uzunluğu */

/* ─── Error Codes ────────────────────────────────────────────────────────── */

typedef enum {
    HAL_OK              =  0,   /* Başarılı */
    HAL_ERR_CONNECT     = -1,   /* Bağlantı kurulamadı */
    HAL_ERR_SEND        = -2,   /* Mesaj gönderilemedi */
    HAL_ERR_RECV        = -3,   /* Yanıt alınamadı */
    HAL_ERR_PROTOCOL    = -4,   /* Protokol hatası (geçersiz JSON vs.) */
    HAL_ERR_NO_MATCH    = -5,   /* Parmak izi eşleşmedi */
    HAL_ERR_NOT_FOUND   = -6,   /* Kayıt bulunamadı */
    HAL_ERR_FULL        = -7,   /* Veritabanı dolu */
    HAL_ERR_EXISTS      = -8,   /* Kayıt zaten var */
    HAL_ERR_DISCONNECTED= -9,   /* Cihaz bağlı değil */
    HAL_ERR_INVALID_ARG = -10,  /* Geçersiz parametre */
    HAL_ERR_TIMEOUT     = -11,  /* Zaman aşımı */
    HAL_ERR_UNKNOWN     = -99,  /* Bilinmeyen hata */
} hal_error_t;

/* ─── Device State ───────────────────────────────────────────────────────── */

typedef enum {
    HAL_STATE_CONNECTED    = 1,
    HAL_STATE_DISCONNECTED = 0,
    HAL_STATE_UNKNOWN      = -1,
} hal_device_state_t;

/* ─── Data Structures ────────────────────────────────────────────────────── */

/**
 * Parmak izi bilgisi.
 */
typedef struct {
    int     finger_id;
    char    name[HAL_MAX_NAME_LEN];
    int     verify_count;
} hal_finger_info_t;

/**
 * Cihaz durumu.
 */
typedef struct {
    hal_device_state_t  state;
    uint8_t             key[HAL_KEY_SIZE];   /* Sadece verify sonrası dolu */
    int                 key_valid;           /* Key geçerli mi? (1/0) */
} hal_status_t;

/* ─── Core API ───────────────────────────────────────────────────────────── */

/**
 * HAL'ı başlat ve cihaza bağlan.
 *
 * @param device_path  Bağlantı hedefi:
 *                     "simulator:/tmp/biometric_vault.sock"  → Simülatör
 *                     "serial:/dev/ttyUSB0"                  → Gerçek HW (sonra)
 * @return HAL_OK veya hata kodu
 */
int hal_init(const char *device_path);

/**
 * HAL'ı kapat, bağlantıyı sonlandır, kaynakları temizle.
 * Key bellekten güvenli şekilde silinir.
 */
void hal_cleanup(void);

/* ─── Fingerprint Operations ─────────────────────────────────────────────── */

/**
 * Parmak izi doğrula ve şifreleme anahtarını al.
 *
 * Bu, sistemin en kritik fonksiyonudur:
 * - Başarılıysa key_out'a AES-256 key yazılır
 * - Başarısızsa key_out'a dokunulmaz
 *
 * @param finger_id  Doğrulanacak parmak izi ID'si (1-127)
 * @param key_out    [out] 32 byte'lık buffer (AES-256 key)
 * @return HAL_OK veya hata kodu (HAL_ERR_NO_MATCH, HAL_ERR_DISCONNECTED, ...)
 */
int hal_verify_finger(int finger_id, uint8_t *key_out);

/**
 * Yeni parmak izi kaydet.
 *
 * @param finger_id  Parmak izi ID'si (1-127)
 * @param name       Parmak izi adı (NULL olabilir)
 * @return HAL_OK veya hata kodu
 */
int hal_enroll_finger(int finger_id, const char *name);

/**
 * Kayıtlı parmak izini sil.
 *
 * @param finger_id  Silinecek parmak izi ID'si
 * @return HAL_OK veya hata kodu
 */
int hal_delete_finger(int finger_id);

/**
 * Kayıtlı parmak izlerini listele.
 *
 * @param fingers    [out] Parmak izi bilgi dizisi
 * @param max_count  Dizinin maksimum kapasitesi
 * @param out_count  [out] Doldurulan eleman sayısı
 * @return HAL_OK veya hata kodu
 */
int hal_list_fingers(hal_finger_info_t *fingers, int max_count, int *out_count);

/* ─── Device Status ──────────────────────────────────────────────────────── */

/**
 * Cihaz durumunu sorgula.
 *
 * @param status  [out] Durum bilgisi
 * @return HAL_OK veya hata kodu
 */
int hal_get_status(hal_status_t *status);

/* ─── Utility ────────────────────────────────────────────────────────────── */

/**
 * Hata kodunu okunabilir string'e çevir.
 *
 * @param err  Hata kodu
 * @return Statik string (free etmeyin)
 */
const char *hal_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* HAL_H */
