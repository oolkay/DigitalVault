/*
 * Biometric Vault — Hardware Abstraction Layer (HAL)
 *
 * Kullanım:
 *   hal_init("simulator:/tmp/biometric_vault.sock")
 *   hal_init("serial:/dev/pts/3")
 *
 * Verify + key (two-step, STM32 model):
 *   hal_verify_finger(&matched_id);
 *   hal_get_key(key_buf);
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_KEY_SIZE        32
#define HAL_MAX_FINGERS     127
#define HAL_MAX_NAME_LEN    64

typedef enum {
    HAL_OK              =  0,
    HAL_ERR_CONNECT     = -1,
    HAL_ERR_SEND        = -2,
    HAL_ERR_RECV        = -3,
    HAL_ERR_PROTOCOL    = -4,
    HAL_ERR_NO_MATCH    = -5,
    HAL_ERR_NOT_FOUND   = -6,
    HAL_ERR_FULL        = -7,
    HAL_ERR_EXISTS      = -8,
    HAL_ERR_DISCONNECTED= -9,
    HAL_ERR_INVALID_ARG = -10,
    HAL_ERR_TIMEOUT     = -11,
    HAL_ERR_UNKNOWN     = -99,
} hal_error_t;

typedef enum {
    HAL_STATE_CONNECTED    = 1,
    HAL_STATE_DISCONNECTED = 0,
    HAL_STATE_UNKNOWN      = -1,
} hal_device_state_t;

typedef struct {
    int     finger_id;
    char    name[HAL_MAX_NAME_LEN];
    int     verify_count;
} hal_finger_info_t;

typedef struct {
    hal_device_state_t  state;
    uint8_t             key[HAL_KEY_SIZE];
    int                 key_valid;
} hal_status_t;

typedef void (*hal_event_cb_t)(uint8_t evt_code, void *user_data);

int  hal_init(const char *device_path);
void hal_cleanup(void);

void hal_set_event_callback(hal_event_cb_t cb, void *user_data);

/** 1:N verify — matched_id_out set on success; key via hal_get_key(). */
int  hal_verify_finger(int *matched_id_out);

/** After successful verify (within ~10s on device). */
int  hal_get_key(uint8_t *key_out);

int  hal_cancel_operation(void);

/** Blocking verify + get_key; wipes key_out on get_key failure. */
int  hal_verify_and_get_key(int *matched_id_out, uint8_t *key_out);

/** finger_id 0 = auto; assigned_slot_out optional (actual slot 1-127 on success). */
int  hal_enroll_finger(int finger_id, const char *name, int *assigned_slot_out);
int  hal_delete_finger(int finger_id);
int  hal_list_fingers(hal_finger_info_t *fingers, int max_count, int *out_count);
int  hal_get_status(hal_status_t *status);

/** Device ping (serial STATUS cmd); template_count_out may be NULL. */
int  hal_ping(int *template_count_out);

const char *hal_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* HAL_H */
