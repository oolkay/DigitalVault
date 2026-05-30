/*
 * Biometric Vault — Binary UART packet codec
 */

#include "hal_protocol.h"
#include "hal.h"
#include <string.h>

uint8_t hal_pkt_checksum(const uint8_t *data, size_t len)
{
    uint8_t x = 0;
    for (size_t i = 0; i < len; i++)
        x ^= data[i];
    return x;
}

int hal_pkt_build(uint8_t code, const uint8_t *payload, uint8_t len,
                  uint8_t *out, size_t out_cap, size_t *out_len)
{
    if (len > HAL_PKT_MAX_PAYLOAD || !out || !out_len)
        return HAL_ERR_INVALID_ARG;

    size_t total = (size_t)4 + len;
    if (out_cap < total)
        return HAL_ERR_INVALID_ARG;

    out[0] = HAL_PKT_SYNC;
    out[1] = code;
    out[2] = len;
    if (len > 0 && payload)
        memcpy(out + 3, payload, len);
    out[3 + len] = hal_pkt_checksum(out, 3 + len);
    *out_len = total;
    return HAL_OK;
}

void hal_rx_reset(hal_rx_t *rx)
{
    memset(rx, 0, sizeof(*rx));
}

int hal_rx_feed(hal_rx_t *rx, uint8_t byte, hal_packet_t *pkt)
{
    if (!rx || !pkt)
        return 0;

    if (!rx->sync_seen) {
        if (byte != HAL_PKT_SYNC)
            return 0;
        rx->sync_seen = 1;
        rx->buf[0] = byte;
        rx->len = 1;
        return 0;
    }

    if (rx->len == 1) {
        rx->code = byte;
        rx->buf[1] = byte;
        rx->len = 2;
        return 0;
    }

    if (rx->len == 2) {
        if (byte > HAL_PKT_MAX_PAYLOAD) {
            hal_rx_reset(rx);
            rx->bad_streak++;
            return 0;
        }
        rx->payload_len = byte;
        rx->buf[2] = byte;
        rx->len = 3;
        rx->payload_pos = 0;
        if (byte == 0) {
            /* wait checksum */
        }
        return 0;
    }

    /* payload or checksum */
    size_t need = 3 + rx->payload_len + 1;
    if (rx->len < need - 1) {
        rx->buf[rx->len++] = byte;
        if (rx->payload_pos < rx->payload_len)
            rx->payload[rx->payload_pos++] = byte;
        return 0;
    }

    /* checksum byte */
    rx->buf[rx->len++] = byte;
    uint8_t expect = hal_pkt_checksum(rx->buf, 3 + rx->payload_len);
    if (byte != expect) {
        hal_rx_reset(rx);
        rx->bad_streak++;
        if (rx->bad_streak >= 3)
            return -1;
        return 0;
    }

    pkt->code = rx->code;
    pkt->len = rx->payload_len;
    if (rx->payload_len > 0)
        memcpy(pkt->payload, rx->payload, rx->payload_len);

    hal_rx_reset(rx);
    rx->bad_streak = 0;
    return 1;
}

int hal_res_to_error(uint8_t res_code)
{
    switch (res_code) {
    case HAL_RES_SUCCESS:
        return HAL_OK;
    case HAL_RES_FAIL_NO_MATCH:
        return HAL_ERR_NO_MATCH;
    case HAL_RES_FAIL_ALREADY_EXISTS:
        return HAL_ERR_EXISTS;
    case HAL_RES_FAIL_SLOT_FULL:
        return HAL_ERR_FULL;
    case HAL_RES_FAIL_INVALID_ID:
        return HAL_ERR_INVALID_ARG;
    case HAL_RES_CANCELLED:
        return HAL_ERR_UNKNOWN;
    case HAL_RES_BUSY:
        return HAL_ERR_UNKNOWN;
    case HAL_RES_SENSOR_ERROR:
        return HAL_ERR_CONNECT;
    case HAL_RES_TIMEOUT:
        return HAL_ERR_TIMEOUT;
    case HAL_RES_FAIL_NO_FINGER:
    case HAL_RES_FAIL_BAD_IMAGE:
    default:
        return HAL_ERR_UNKNOWN;
    }
}
