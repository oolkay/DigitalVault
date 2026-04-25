/*
 * Biometric Vault — HAL Common Utilities
 *
 * Backend'den bağımsız yardımcı fonksiyonlar.
 */

#include "hal.h"
#include <string.h>
#include <stdio.h>

/* ─── Error Strings ──────────────────────────────────────────────────────── */

const char *hal_strerror(int err)
{
    switch (err) {
    case HAL_OK:                return "Success";
    case HAL_ERR_CONNECT:       return "Connection failed";
    case HAL_ERR_SEND:          return "Send failed";
    case HAL_ERR_RECV:          return "Receive failed";
    case HAL_ERR_PROTOCOL:      return "Protocol error";
    case HAL_ERR_NO_MATCH:      return "Fingerprint no match";
    case HAL_ERR_NOT_FOUND:     return "Record not found";
    case HAL_ERR_FULL:          return "Database full";
    case HAL_ERR_EXISTS:        return "Record already exists";
    case HAL_ERR_DISCONNECTED:  return "Device disconnected";
    case HAL_ERR_INVALID_ARG:   return "Invalid argument";
    case HAL_ERR_TIMEOUT:       return "Timeout";
    default:                    return "Unknown error";
    }
}
