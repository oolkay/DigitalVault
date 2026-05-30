/* Mirror of DigitalVault hal/hal_protocol.h for STM32 firmware */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define HAL_PKT_SYNC 0xAA
#define HAL_PKT_MAX_PAYLOAD 64
#define HAL_PKT_MAX_SIZE (4 + HAL_PKT_MAX_PAYLOAD)
#define HAL_KEY_SIZE 32

#define HAL_CMD_ENROLL_START 0x01
#define HAL_CMD_VERIFY_START 0x02
#define HAL_CMD_DELETE 0x03
#define HAL_CMD_CANCEL 0x04
#define HAL_CMD_GET_KEY 0x05
#define HAL_CMD_LIST_ENROLLED 0x06
#define HAL_CMD_STATUS        0x07

#define HAL_EVT_WAITING_FINGER 0x10
#define HAL_EVT_FINGER_DETECTED 0x11
#define HAL_EVT_REMOVE_FINGER 0x12
#define HAL_EVT_PLACE_AGAIN 0x13
#define HAL_EVT_PROCESSING 0x14

#define HAL_RES_SUCCESS 0x20
#define HAL_RES_FAIL_NO_FINGER 0x21
#define HAL_RES_FAIL_BAD_IMAGE 0x22
#define HAL_RES_FAIL_NO_MATCH 0x23
#define HAL_RES_FAIL_ALREADY_EXISTS 0x24
#define HAL_RES_FAIL_SLOT_FULL 0x25
#define HAL_RES_FAIL_INVALID_ID 0x26
#define HAL_RES_CANCELLED 0x27
#define HAL_RES_TIMEOUT 0x28
#define HAL_RES_KEY_DATA 0x29
#define HAL_RES_BUSY 0x2A
#define HAL_RES_SENSOR_ERROR 0x2B

typedef struct {
    uint8_t code;
    uint8_t payload[HAL_PKT_MAX_PAYLOAD];
    uint8_t len;
} HalPacket;

typedef struct {
    uint8_t sync_seen;
    uint8_t code;
    uint8_t payload_len;
    uint8_t payload[HAL_PKT_MAX_PAYLOAD];
    size_t payload_pos;
    uint8_t bad_streak;
    uint8_t rx_len;
} HalRx;

uint8_t halPktChecksum(const uint8_t *data, size_t len);
size_t halPktBuild(uint8_t code, const uint8_t *payload, uint8_t plen, uint8_t *out,
                   size_t outCap);
void halRxReset(HalRx *rx);
/* 1 = complete packet, 0 = need more, -1 = protocol fault streak */
int halRxFeed(HalRx *rx, uint8_t byte, HalPacket *pkt);
