/*
 * HAL backend dispatch (internal)
 */

#ifndef HAL_INTERNAL_H
#define HAL_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include "hal.h"

typedef void (*hal_event_cb_t)(uint8_t evt_code, void *user_data);

typedef struct hal_backend {
    int  (*init)(const char *path);
    void (*cleanup)(void);
    int  (*verify)(int *matched_id_out);
    int  (*get_key)(uint8_t *key_out);
    int  (*cancel)(void);
    int  (*enroll)(int finger_id, const char *name, int *assigned_slot_out);
    int  (*delete_finger)(int finger_id);
    int  (*list)(hal_finger_info_t *fingers, int max_count, int *out_count);
    int  (*status)(hal_status_t *status);
    int  (*ping)(int *template_count_out);
} hal_backend_t;

extern const hal_backend_t *g_hal_backend;

void hal_secure_wipe(void *ptr, size_t len);

void hal_emit_event(uint8_t evt_code);

void hal_set_event_callback(hal_event_cb_t cb, void *user_data);

extern const hal_backend_t hal_sim_backend;
extern const hal_backend_t hal_serial_backend;

#endif /* HAL_INTERNAL_H */
