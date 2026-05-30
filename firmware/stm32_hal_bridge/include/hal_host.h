#pragma once

#include "hal_proto.h"

void halHostSend(uint8_t code, const uint8_t *payload, uint8_t plen);
void halHostSendEvent(uint8_t evt);
void halHostSendResult(uint8_t res, const uint8_t *payload, uint8_t plen);
void halHostPollRx(void (*handler)(const HalPacket *pkt));

#ifdef HAL_DIAG_BUILD
#include <stddef.h>
void halHostSetDiagTxHook(void (*hook)(const uint8_t *data, size_t len));
#endif
