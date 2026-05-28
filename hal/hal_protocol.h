/*
 * Biometric Vault — Binary UART packet codec (PC ↔ STM32)
 */

#ifndef HAL_PROTOCOL_H
#define HAL_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define HAL_PKT_SYNC          0xAA
#define HAL_PKT_MAX_PAYLOAD   64
#define HAL_PKT_MAX_SIZE      (4 + HAL_PKT_MAX_PAYLOAD)

/* PC → STM */
#define HAL_CMD_ENROLL_START  0x01
#define HAL_CMD_VERIFY_START  0x02
#define HAL_CMD_DELETE        0x03
#define HAL_CMD_CANCEL        0x04
#define HAL_CMD_GET_KEY       0x05
#define HAL_CMD_LIST_ENROLLED 0x06  /* STM32: AS608 template scan → [count][id…] */

/* STM → PC events */
#define HAL_EVT_WAITING_FINGER 0x10
#define HAL_EVT_FINGER_DETECTED 0x11
#define HAL_EVT_REMOVE_FINGER  0x12
#define HAL_EVT_PLACE_AGAIN    0x13
#define HAL_EVT_PROCESSING     0x14

/* STM → PC results */
#define HAL_RES_SUCCESS           0x20
#define HAL_RES_FAIL_NO_FINGER    0x21
#define HAL_RES_FAIL_BAD_IMAGE    0x22
#define HAL_RES_FAIL_NO_MATCH     0x23
#define HAL_RES_FAIL_ALREADY_EXISTS 0x24
#define HAL_RES_FAIL_SLOT_FULL    0x25
#define HAL_RES_FAIL_INVALID_ID   0x26
#define HAL_RES_CANCELLED         0x27
#define HAL_RES_TIMEOUT           0x28
#define HAL_RES_KEY_DATA          0x29

typedef struct {
    uint8_t  code;
    uint8_t  payload[HAL_PKT_MAX_PAYLOAD];
    uint8_t  len;
} hal_packet_t;

typedef struct {
    uint8_t  buf[HAL_PKT_MAX_SIZE * 2];
    size_t   len;
    int      sync_seen;
    uint8_t  code;
    uint8_t  payload_len;
    uint8_t  payload[HAL_PKT_MAX_PAYLOAD];
    size_t   payload_pos;
    int      bad_streak;
} hal_rx_t;

uint8_t hal_pkt_checksum(const uint8_t *data, size_t len);

int hal_pkt_build(uint8_t code, const uint8_t *payload, uint8_t len,
                  uint8_t *out, size_t out_cap, size_t *out_len);

void hal_rx_reset(hal_rx_t *rx);

/* Feed one byte; returns 1 when a full valid packet is in *pkt */
int hal_rx_feed(hal_rx_t *rx, uint8_t byte, hal_packet_t *pkt);

int hal_res_to_error(uint8_t res_code);

#endif /* HAL_PROTOCOL_H */
