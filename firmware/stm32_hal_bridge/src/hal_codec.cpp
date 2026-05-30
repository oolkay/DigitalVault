#include "hal_proto.h"
#ifdef HAL_DIAG_BUILD
#include "hal_host.h"
#endif
#include <Arduino.h>
#include <string.h>

HardwareSerial &hostSerial();

uint8_t halPktChecksum(const uint8_t *data, size_t len) {
    uint8_t x = 0;
    for (size_t i = 0; i < len; i++)
        x ^= data[i];
    return x;
}

size_t halPktBuild(uint8_t code, const uint8_t *payload, uint8_t plen, uint8_t *out,
                   size_t outCap) {
    if (plen > HAL_PKT_MAX_PAYLOAD)
        return 0;
    size_t total = 4 + plen;
    if (outCap < total)
        return 0;
    out[0] = HAL_PKT_SYNC;
    out[1] = code;
    out[2] = plen;
    if (plen > 0 && payload)
        memcpy(out + 3, payload, plen);
    out[3 + plen] = halPktChecksum(out, 3 + plen);
    return total;
}

void halRxReset(HalRx *rx) { memset(rx, 0, sizeof(*rx)); }

int halRxFeed(HalRx *rx, uint8_t byte, HalPacket *pkt) {
    if (!rx->sync_seen) {
        if (byte != HAL_PKT_SYNC)
            return 0;
        rx->sync_seen = 1;
        rx->rx_len = 1;
        return 0;
    }
    if (rx->rx_len == 1) {
        rx->code = byte;
        rx->rx_len = 2;
        return 0;
    }
    if (rx->rx_len == 2) {
        if (byte > HAL_PKT_MAX_PAYLOAD) {
            halRxReset(rx);
            if (++rx->bad_streak >= 3)
                return -1;
            return 0;
        }
        rx->payload_len = byte;
        rx->payload_pos = 0;
        rx->rx_len = 3;
        return 0;
    }
    
    size_t need = 3 + rx->payload_len + 1;
    if (rx->rx_len < need - 1) {
        if (rx->payload_pos < rx->payload_len) {
            rx->payload[rx->payload_pos++] = byte;
        }
        rx->rx_len++;
        return 0;
    }

    uint8_t body[3 + HAL_PKT_MAX_PAYLOAD];
    body[0] = HAL_PKT_SYNC;
    body[1] = rx->code;
    body[2] = rx->payload_len;
    if (rx->payload_len > 0)
        memcpy(body + 3, rx->payload, rx->payload_len);
    if (byte != halPktChecksum(body, 3 + rx->payload_len)) {
        halRxReset(rx);
        if (++rx->bad_streak >= 3)
            return -1;
        return 0;
    }
    pkt->code = rx->code;
    pkt->len = rx->payload_len;
    if (rx->payload_len > 0)
        memcpy(pkt->payload, rx->payload, rx->payload_len);
    halRxReset(rx);
    rx->bad_streak = 0;
    return 1;
}

static uint8_t txBuf[HAL_PKT_MAX_SIZE];

#ifdef HAL_DIAG_BUILD
static void (*g_diagTxHook)(const uint8_t *data, size_t len) = nullptr;

void halHostSetDiagTxHook(void (*hook)(const uint8_t *data, size_t len)) { g_diagTxHook = hook; }
#endif

void halHostSend(uint8_t code, const uint8_t *payload, uint8_t plen) {
    size_t n = halPktBuild(code, payload, plen, txBuf, sizeof(txBuf));
    if (n > 0) {
#ifdef HAL_DIAG_BUILD
        if (g_diagTxHook) {
            g_diagTxHook(txBuf, n);
            return;
        }
#endif
        hostSerial().write(txBuf, n);
        hostSerial().flush();
    }
}

void halHostSendEvent(uint8_t evt) { halHostSend(evt, nullptr, 0); }

void halHostSendResult(uint8_t res, const uint8_t *payload, uint8_t plen) {
    halHostSend(res, payload, plen);
}

void halHostPollRx(void (*handler)(const HalPacket *pkt)) {
    static HalRx rx;
    HalPacket pkt;
    while (hostSerial().available() > 0) {
        int r = halRxFeed(&rx, (uint8_t)hostSerial().read(), &pkt);
        if (r < 0) {
            halRxReset(&rx);
            continue;
        }
        if (r == 1 && handler)
            handler(&pkt);
    }
}
