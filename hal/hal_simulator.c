/*
 * Biometric Vault — HAL Simulator Backend
 *
 * Python simulator daemon'a Unix Domain Socket üzerinden bağlanır.
 * JSON mesaj protokolünü kullanır.
 *
 * Donanım geldiğinde bu dosya yerine hal_serial.c kullanılacak.
 * hal.h API'si değişmeyecek.
 *
 * Bağımlılıklar: cJSON (JSON parse için)
 *   sudo apt install libcjson-dev
 */

#include "hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include <cjson/cJSON.h>

/* Base64 decode için OpenSSL */
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

/* ─── Internal State ─────────────────────────────────────────────────────── */

static char g_socket_path[108] = {0}; /* Unix socket path limiti genellikle 108'dir */
static int  g_initialized = 0;

/* ─── Base64 Decode ──────────────────────────────────────────────────────── */

/**
 * Base64 string'i binary'ye çevir.
 * @return Decode edilen byte sayısı, hata durumunda -1
 */
static int base64_decode(const char *b64_input, uint8_t *output, int out_max)
{
    BIO *bio, *b64;
    int decoded_len;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(b64_input, -1);
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    decoded_len = BIO_read(bio, output, out_max);

    BIO_free_all(bio);
    return decoded_len;
}

/* ─── Socket Communication ───────────────────────────────────────────────── */

/**
 * Simulator'a bağlan, mesaj gönder, yanıt al.
 * Her komut için yeni bağlantı açılır (simulator'un çalışma modeli bu).
 *
 * @param request_json  Gönderilecek JSON string (\n ile bitmeli)
 * @param response_buf  Yanıt buffer'ı
 * @param buf_size      Buffer boyutu
 * @return HAL_OK veya hata kodu
 */
static int send_recv(const char *request_json, char *response_buf, size_t buf_size)
{
    int sock;
    struct sockaddr_un addr;
    ssize_t n;

    /* Socket oluştur */
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "[HAL] socket() failed: %s\n", strerror(errno));
        return HAL_ERR_CONNECT;
    }

    /* Bağlan */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    /* strncpy yerine snprintf daha güvenlidir ve otomatik null-terminate eder */
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", g_socket_path);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[HAL] connect(%s) failed: %s\n", g_socket_path, strerror(errno));
        close(sock);
        return HAL_ERR_CONNECT;
    }

    /* İstek gönder */
    size_t req_len = strlen(request_json);
    n = write(sock, request_json, req_len);
    if (n < 0 || (size_t)n != req_len) {
        fprintf(stderr, "[HAL] write() failed: %s\n", strerror(errno));
        close(sock);
        return HAL_ERR_SEND;
    }

    /* Yanıt oku */
    memset(response_buf, 0, buf_size);
    size_t total = 0;
    while (total < buf_size - 1) {
        n = read(sock, response_buf + total, buf_size - 1 - total);
        if (n <= 0)
            break;
        total += n;
        /* Newline ile sonlandırılmış mı kontrol et */
        if (response_buf[total - 1] == '\n')
            break;
    }

    close(sock);

    if (total == 0) {
        fprintf(stderr, "[HAL] Empty response from simulator\n");
        return HAL_ERR_RECV;
    }

    return HAL_OK;
}

/* ─── JSON Helpers ───────────────────────────────────────────────────────── */

/**
 * Yanıttaki "reason" alanından uygun hata kodunu belirle.
 */
static int reason_to_error(const char *reason)
{
    if (!reason)
        return HAL_ERR_UNKNOWN;
    if (strcmp(reason, "no_match") == 0)
        return HAL_ERR_NO_MATCH;
    if (strcmp(reason, "not_found") == 0)
        return HAL_ERR_NOT_FOUND;
    if (strcmp(reason, "device_disconnected") == 0)
        return HAL_ERR_DISCONNECTED;
    if (strstr(reason, "already enrolled") != NULL)
        return HAL_ERR_EXISTS;
    if (strstr(reason, "full") != NULL)
        return HAL_ERR_FULL;
    if (strstr(reason, "must be between") != NULL)
        return HAL_ERR_INVALID_ARG;
    return HAL_ERR_UNKNOWN;
}

/**
 * JSON formatında istek oluştur, gönder, yanıtı parse et.
 *
 * @param cmd       Komut adı
 * @param root_out  [out] Parse edilmiş yanıt JSON'u (çağıran cJSON_Delete ile serbest bırakmalı)
 * @return HAL_OK veya hata kodu
 */
static int execute_command(cJSON *request, cJSON **root_out)
{
    char *req_str = NULL;
    char response_buf[4096];
    int ret;

    /* JSON'u string'e çevir ve newline ekle */
    req_str = cJSON_PrintUnformatted(request);
    if (!req_str) {
        return HAL_ERR_PROTOCOL;
    }

    /* Newline eklenmiş request oluştur */
    size_t len = strlen(req_str);
    char *req_with_nl = malloc(len + 2);
    if (!req_with_nl) {
        free(req_str);
        return HAL_ERR_PROTOCOL;
    }
    memcpy(req_with_nl, req_str, len);
    req_with_nl[len] = '\n';
    req_with_nl[len + 1] = '\0';
    free(req_str);

    /* Gönder ve al */
    ret = send_recv(req_with_nl, response_buf, sizeof(response_buf));
    free(req_with_nl);

    if (ret != HAL_OK) {
        return ret;
    }

    /* Yanıtı parse et */
    cJSON *root = cJSON_Parse(response_buf);
    if (!root) {
        fprintf(stderr, "[HAL] JSON parse error: %s\n", response_buf);
        return HAL_ERR_PROTOCOL;
    }

    /* Status kontrolü */
    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || !cJSON_IsString(status)) {
        cJSON_Delete(root);
        return HAL_ERR_PROTOCOL;
    }

    if (strcmp(status->valuestring, "ok") != 0) {
        /* Hata yanıtı */
        cJSON *reason = cJSON_GetObjectItem(root, "reason");
        const char *reason_str = (reason && cJSON_IsString(reason)) ? reason->valuestring : NULL;
        ret = reason_to_error(reason_str);
        cJSON_Delete(root);
        return ret;
    }

    *root_out = root;
    return HAL_OK;
}

/* ─── Public API Implementation ──────────────────────────────────────────── */

int hal_init(const char *device_path)
{
    /*
     * device_path formatı: "simulator:/tmp/biometric_vault.sock"
     *
     * Prefix'e göre backend seçilir:
     *   "simulator:" → Unix socket (bu dosya)
     *   "serial:"    → UART (hal_serial.c — sonra)
     */

    if (!device_path) {
        return HAL_ERR_INVALID_ARG;
    }

    const char *prefix = "simulator:";
    size_t prefix_len = strlen(prefix);

    if (strncmp(device_path, prefix, prefix_len) != 0) {
        fprintf(stderr, "[HAL] Unknown backend: %s\n", device_path);
        fprintf(stderr, "[HAL] Expected: simulator:<socket_path>\n");
        return HAL_ERR_INVALID_ARG;
    }

    const char *sock_path = device_path + prefix_len;
    snprintf(g_socket_path, sizeof(g_socket_path), "%s", sock_path);
    g_initialized = 1;

    fprintf(stderr, "[HAL] Initialized — backend=simulator, socket=%s\n", g_socket_path);

    /* Bağlantı testi */
    hal_status_t st;
    int ret = hal_get_status(&st);
    if (ret != HAL_OK) {
        fprintf(stderr, "[HAL] WARNING: Initial status check failed (%s)\n", hal_strerror(ret));
        fprintf(stderr, "[HAL] Is the simulator running?\n");
    } else {
        fprintf(stderr, "[HAL] Device state: %s\n",
                st.state == HAL_STATE_CONNECTED ? "CONNECTED" : "DISCONNECTED");
    }

    return HAL_OK;
}

void hal_cleanup(void)
{
    /* Bellekteki hassas verileri temizle */
    memset(g_socket_path, 0, sizeof(g_socket_path));
    g_initialized = 0;
    fprintf(stderr, "[HAL] Cleaned up\n");
}

int hal_verify_finger(int finger_id, uint8_t *key_out)
{
    if (!g_initialized) return HAL_ERR_CONNECT;
    if (!key_out)        return HAL_ERR_INVALID_ARG;
    if (finger_id < 1 || finger_id > HAL_MAX_FINGERS) return HAL_ERR_INVALID_ARG;

    /* JSON request oluştur */
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "verify");
    cJSON_AddNumberToObject(req, "finger_id", finger_id);

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);

    if (ret != HAL_OK) {
        return ret;
    }

    /* Key'i al ve decode et */
    cJSON *key_item = cJSON_GetObjectItem(resp, "key");
    if (!key_item || !cJSON_IsString(key_item)) {
        cJSON_Delete(resp);
        return HAL_ERR_PROTOCOL;
    }

    int decoded = base64_decode(key_item->valuestring, key_out, HAL_KEY_SIZE);
    cJSON_Delete(resp);

    if (decoded != HAL_KEY_SIZE) {
        fprintf(stderr, "[HAL] Key decode error: expected %d bytes, got %d\n",
                HAL_KEY_SIZE, decoded);
        /* Güvenlik: hatalı key'i temizle */
        memset(key_out, 0, HAL_KEY_SIZE);
        return HAL_ERR_PROTOCOL;
    }

    return HAL_OK;
}

int hal_enroll_finger(int finger_id, const char *name)
{
    if (!g_initialized) return HAL_ERR_CONNECT;
    if (finger_id < 1 || finger_id > HAL_MAX_FINGERS) return HAL_ERR_INVALID_ARG;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "enroll");
    cJSON_AddNumberToObject(req, "finger_id", finger_id);
    if (name) {
        cJSON_AddStringToObject(req, "name", name);
    }

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (resp) cJSON_Delete(resp);

    return ret;
}

int hal_delete_finger(int finger_id)
{
    if (!g_initialized) return HAL_ERR_CONNECT;
    if (finger_id < 1 || finger_id > HAL_MAX_FINGERS) return HAL_ERR_INVALID_ARG;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "delete");
    cJSON_AddNumberToObject(req, "finger_id", finger_id);

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (resp) cJSON_Delete(resp);

    return ret;
}

int hal_list_fingers(hal_finger_info_t *fingers, int max_count, int *out_count)
{
    if (!g_initialized) return HAL_ERR_CONNECT;
    if (!fingers || !out_count || max_count <= 0) return HAL_ERR_INVALID_ARG;

    *out_count = 0;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "list");

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);

    if (ret != HAL_OK) {
        return ret;
    }

    /* Parmak izi listesini parse et */
    cJSON *arr = cJSON_GetObjectItem(resp, "fingers");
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(resp);
        return HAL_ERR_PROTOCOL;
    }

    int count = cJSON_GetArraySize(arr);
    if (count > max_count) {
        count = max_count;
    }

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *id   = cJSON_GetObjectItem(item, "finger_id");
        cJSON *name = cJSON_GetObjectItem(item, "name");
        cJSON *vc   = cJSON_GetObjectItem(item, "verify_count");

        fingers[i].finger_id = id ? id->valueint : 0;
        fingers[i].verify_count = vc ? vc->valueint : 0;

        if (name && cJSON_IsString(name)) {
            strncpy(fingers[i].name, name->valuestring, HAL_MAX_NAME_LEN - 1);
            fingers[i].name[HAL_MAX_NAME_LEN - 1] = '\0';
        } else {
            snprintf(fingers[i].name, HAL_MAX_NAME_LEN, "Finger_%d",
                     fingers[i].finger_id);
        }
    }

    *out_count = count;
    cJSON_Delete(resp);
    return HAL_OK;
}

int hal_get_status(hal_status_t *status)
{
    if (!g_initialized) return HAL_ERR_CONNECT;
    if (!status)        return HAL_ERR_INVALID_ARG;

    memset(status, 0, sizeof(*status));
    status->state = HAL_STATE_UNKNOWN;
    status->key_valid = 0;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "status");

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);

    if (ret != HAL_OK) {
        return ret;
    }

    cJSON *ds = cJSON_GetObjectItem(resp, "device_state");
    if (ds && cJSON_IsString(ds)) {
        if (strcmp(ds->valuestring, "connected") == 0)
            status->state = HAL_STATE_CONNECTED;
        else
            status->state = HAL_STATE_DISCONNECTED;
    }

    cJSON_Delete(resp);
    return HAL_OK;
}
