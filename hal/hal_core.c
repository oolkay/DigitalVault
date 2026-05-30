/*
 * HAL public API — backend dispatch
 */

#include "hal.h"
#include "hal_internal.h"
#include <stdio.h>
#include <string.h>

const hal_backend_t *g_hal_backend = NULL;

static hal_event_cb_t g_event_cb = NULL;
static void *g_event_user = NULL;

void hal_secure_wipe(void *ptr, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--)
        *p++ = 0;
}

void hal_set_event_callback(hal_event_cb_t cb, void *user_data)
{
    g_event_cb = cb;
    g_event_user = user_data;
}

void hal_emit_event(uint8_t evt_code)
{
    if (g_event_cb)
        g_event_cb(evt_code, g_event_user);
}

int hal_init(const char *device_path)
{
    if (!device_path)
        return HAL_ERR_INVALID_ARG;

    if (g_hal_backend) {
        g_hal_backend->cleanup();
        g_hal_backend = NULL;
    }

    const char *sim = "simulator:";
    const char *ser = "serial:";

    if (strncmp(device_path, sim, strlen(sim)) == 0) {
        g_hal_backend = &hal_sim_backend;
        return g_hal_backend->init(device_path + strlen(sim));
    }
    if (strncmp(device_path, ser, strlen(ser)) == 0) {
        g_hal_backend = &hal_serial_backend;
        return g_hal_backend->init(device_path + strlen(ser));
    }

    fprintf(stderr, "[HAL] Unknown backend in: %s\n", device_path);
    fprintf(stderr, "[HAL] Use simulator:<socket> or serial:<device>\n");
    return HAL_ERR_INVALID_ARG;
}

void hal_cleanup(void)
{
    if (g_hal_backend) {
        g_hal_backend->cleanup();
        g_hal_backend = NULL;
    }
}

int hal_verify_finger(int *matched_id_out)
{
    if (!g_hal_backend)
        return HAL_ERR_CONNECT;
    if (!matched_id_out)
        return HAL_ERR_INVALID_ARG;
    return g_hal_backend->verify(matched_id_out);
}

int hal_get_key(uint8_t *key_out)
{
    if (!g_hal_backend)
        return HAL_ERR_CONNECT;
    if (!key_out)
        return HAL_ERR_INVALID_ARG;
    return g_hal_backend->get_key(key_out);
}

int hal_cancel_operation(void)
{
    if (!g_hal_backend)
        return HAL_ERR_CONNECT;
    return g_hal_backend->cancel();
}

int hal_enroll_finger(int finger_id, const char *name, int *assigned_slot_out)
{
    if (!g_hal_backend)
        return HAL_ERR_CONNECT;
    return g_hal_backend->enroll(finger_id, name, assigned_slot_out);
}

int hal_delete_finger(int finger_id)
{
    if (!g_hal_backend)
        return HAL_ERR_CONNECT;
    return g_hal_backend->delete_finger(finger_id);
}

int hal_list_fingers(hal_finger_info_t *fingers, int max_count, int *out_count)
{
    if (!g_hal_backend)
        return HAL_ERR_CONNECT;
    return g_hal_backend->list(fingers, max_count, out_count);
}

int hal_get_status(hal_status_t *status)
{
    if (!g_hal_backend)
        return HAL_ERR_CONNECT;
    return g_hal_backend->status(status);
}

int hal_ping(int *template_count_out)
{
    if (!g_hal_backend)
        return HAL_ERR_CONNECT;
    if (!g_hal_backend->ping)
        return HAL_ERR_INVALID_ARG;
    return g_hal_backend->ping(template_count_out);
}

/* Convenience: verify + get_key with wipe on failure */
int hal_verify_and_get_key(int *matched_id_out, uint8_t *key_out)
{
    int ret = hal_verify_finger(matched_id_out);
    if (ret != HAL_OK)
        return ret;
    ret = hal_get_key(key_out);
    if (ret != HAL_OK)
        hal_secure_wipe(key_out, HAL_KEY_SIZE);
    return ret;
}
