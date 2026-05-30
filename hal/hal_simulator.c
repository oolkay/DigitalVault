/*
 * HAL — JSON Unix-socket simulator backend
 */

#include "hal_internal.h"
#include <cjson/cJSON.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static char g_socket_path[256];
static int  g_initialized = 0;

static int base64_decode(const char *in, uint8_t *out, size_t out_len)
{
    int len = (int)strlen(in);
    int decoded = EVP_DecodeBlock(out, (const unsigned char *)in, len);
    if (decoded < 0)
        return -1;
    while (len > 0 && in[len - 1] == '=') {
        decoded--;
        len--;
    }
    return (decoded == (int)out_len) ? decoded : -1;
}

static int send_recv(const char *request, char *response, size_t resp_size)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
        return HAL_ERR_CONNECT;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return HAL_ERR_CONNECT;
    }

    size_t req_len = strlen(request);
    ssize_t sent = 0;
    while ((size_t)sent < req_len) {
        ssize_t n = write(sock, request + sent, req_len - sent);
        if (n <= 0) {
            close(sock);
            return HAL_ERR_SEND;
        }
        sent += n;
    }

    size_t total = 0;
    while (total < resp_size - 1) {
        ssize_t n = read(sock, response + total, resp_size - 1 - total);
        if (n <= 0)
            break;
        total += (size_t)n;
        if (memchr(response, '\n', total))
            break;
    }
    close(sock);
    response[total] = '\0';
    char *nl = strchr(response, '\n');
    if (nl)
        *nl = '\0';
    if (total == 0)
        return HAL_ERR_RECV;
    return HAL_OK;
}

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
    if (strstr(reason, "key") != NULL && strstr(reason, "window") != NULL)
        return HAL_ERR_PROTOCOL;
    return HAL_ERR_UNKNOWN;
}

static int execute_command(cJSON *request, cJSON **root_out)
{
    char *req_str = cJSON_PrintUnformatted(request);
    if (!req_str)
        return HAL_ERR_PROTOCOL;

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

    char response_buf[4096];
    int ret = send_recv(req_with_nl, response_buf, sizeof(response_buf));
    free(req_with_nl);
    if (ret != HAL_OK)
        return ret;

    cJSON *root = cJSON_Parse(response_buf);
    if (!root)
        return HAL_ERR_PROTOCOL;

    cJSON *status = cJSON_GetObjectItem(root, "status");
    if (!status || !cJSON_IsString(status)) {
        cJSON_Delete(root);
        return HAL_ERR_PROTOCOL;
    }
    if (strcmp(status->valuestring, "ok") != 0) {
        cJSON *reason = cJSON_GetObjectItem(root, "reason");
        const char *rs = (reason && cJSON_IsString(reason)) ? reason->valuestring : NULL;
        ret = reason_to_error(rs);
        cJSON_Delete(root);
        return ret;
    }
    *root_out = root;
    return HAL_OK;
}

static int sim_init(const char *sock_path)
{
    if (!sock_path || !*sock_path)
        return HAL_ERR_INVALID_ARG;
    snprintf(g_socket_path, sizeof(g_socket_path), "%s", sock_path);
    g_initialized = 1;
    fprintf(stderr, "[HAL] Simulator socket: %s\n", g_socket_path);
    return HAL_OK;
}

static void sim_cleanup(void)
{
    hal_secure_wipe(g_socket_path, sizeof(g_socket_path));
    g_initialized = 0;
}

static int sim_verify(int *matched_id_out)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "verify_start");

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (ret != HAL_OK)
        return ret;

    cJSON *fid = cJSON_GetObjectItem(resp, "finger_id");
    if (!fid || !cJSON_IsNumber(fid)) {
        cJSON_Delete(resp);
        return HAL_ERR_PROTOCOL;
    }
    *matched_id_out = fid->valueint;
    cJSON_Delete(resp);
    return HAL_OK;
}

static int sim_get_key(uint8_t *key_out)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "get_key");

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (ret != HAL_OK)
        return ret;

    cJSON *key_item = cJSON_GetObjectItem(resp, "key");
    if (!key_item || !cJSON_IsString(key_item)) {
        cJSON_Delete(resp);
        return HAL_ERR_PROTOCOL;
    }
    int decoded = base64_decode(key_item->valuestring, key_out, HAL_KEY_SIZE);
    cJSON_Delete(resp);
    if (decoded != HAL_KEY_SIZE) {
        hal_secure_wipe(key_out, HAL_KEY_SIZE);
        return HAL_ERR_PROTOCOL;
    }
    return HAL_OK;
}

static int sim_cancel(void)
{
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "cancel");
    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (resp)
        cJSON_Delete(resp);
    return ret;
}

static int sim_enroll(int finger_id, const char *name, int *assigned_slot_out)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    if (finger_id < 0 || finger_id > HAL_MAX_FINGERS)
        return HAL_ERR_INVALID_ARG;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "enroll");
    cJSON_AddNumberToObject(req, "finger_id", finger_id);
    if (name)
        cJSON_AddStringToObject(req, "name", name);

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (ret == HAL_OK && assigned_slot_out && resp) {
        cJSON *fid = cJSON_GetObjectItem(resp, "finger_id");
        if (fid && cJSON_IsNumber(fid))
            *assigned_slot_out = fid->valueint;
    }
    if (resp)
        cJSON_Delete(resp);
    return ret;
}

static int sim_delete(int finger_id)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    if (finger_id < 1 || finger_id > HAL_MAX_FINGERS)
        return HAL_ERR_INVALID_ARG;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "delete");
    cJSON_AddNumberToObject(req, "finger_id", finger_id);

    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (resp)
        cJSON_Delete(resp);
    return ret;
}

static int sim_list(hal_finger_info_t *fingers, int max_count, int *out_count)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    if (!fingers || !out_count || max_count <= 0)
        return HAL_ERR_INVALID_ARG;
    *out_count = 0;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "list");
    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (ret != HAL_OK)
        return ret;

    cJSON *arr = cJSON_GetObjectItem(resp, "fingers");
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(resp);
        return HAL_ERR_PROTOCOL;
    }
    int count = cJSON_GetArraySize(arr);
    if (count > max_count)
        count = max_count;
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *id = cJSON_GetObjectItem(item, "finger_id");
        cJSON *name = cJSON_GetObjectItem(item, "name");
        cJSON *vc = cJSON_GetObjectItem(item, "verify_count");
        fingers[i].finger_id = id ? id->valueint : 0;
        fingers[i].verify_count = vc ? vc->valueint : 0;
        if (name && cJSON_IsString(name)) {
            strncpy(fingers[i].name, name->valuestring, HAL_MAX_NAME_LEN - 1);
            fingers[i].name[HAL_MAX_NAME_LEN - 1] = '\0';
        } else {
            snprintf(fingers[i].name, HAL_MAX_NAME_LEN, "Finger_%d", fingers[i].finger_id);
        }
    }
    *out_count = count;
    cJSON_Delete(resp);
    return HAL_OK;
}

static int sim_status(hal_status_t *status)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    if (!status)
        return HAL_ERR_INVALID_ARG;
    memset(status, 0, sizeof(*status));
    status->state = HAL_STATE_UNKNOWN;

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "cmd", "status");
    cJSON *resp = NULL;
    int ret = execute_command(req, &resp);
    cJSON_Delete(req);
    if (ret != HAL_OK)
        return ret;

    cJSON *ds = cJSON_GetObjectItem(resp, "device_state");
    if (ds && cJSON_IsString(ds)) {
        status->state = (strcmp(ds->valuestring, "connected") == 0)
                            ? HAL_STATE_CONNECTED
                            : HAL_STATE_DISCONNECTED;
    }
    cJSON_Delete(resp);
    return HAL_OK;
}

static int sim_ping(int *template_count_out)
{
    if (!g_initialized)
        return HAL_ERR_CONNECT;
    if (template_count_out)
        *template_count_out = 0;
    return HAL_OK;
}

const hal_backend_t hal_sim_backend = {
    .init = sim_init,
    .cleanup = sim_cleanup,
    .verify = sim_verify,
    .get_key = sim_get_key,
    .cancel = sim_cancel,
    .enroll = sim_enroll,
    .delete_finger = sim_delete,
    .list = sim_list,
    .status = sim_status,
    .ping = sim_ping,
};
