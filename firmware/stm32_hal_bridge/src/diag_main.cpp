/**
 * DV standalone diag — ASCII CLI on USART2 @ 115200.
 * Production HAL (bluepill_hal) expects binary packets; use this env to test MCU+AS608.
 *
 * P probe | C count | L list | E0 enroll auto | E5 slot5 | V verify | D5 delete
 * Q cancel | Hp HAL STATUS | Hl HAL list | Hv HAL verify | ? help
 */
#include "hal_host.h"
#include "hal_proto.h"
#include "operation_fsm.h"
#include "sensor_service.h"
#include <Adafruit_Fingerprint.h>
#include <Arduino.h>
#include <ctype.h>
#include <string.h>

static HardwareSerial Host(PA3, PA2);
static HardwareSerial Sensor(PB11, PB10);

HardwareSerial &hostSerial() { return Host; }

static uint8_t g_lastTxCode = 0;
static uint8_t g_lastTxPayload[HAL_PKT_MAX_PAYLOAD];
static uint8_t g_lastTxLen = 0;

static void diagTxHook(const uint8_t *data, size_t len) {
    Host.print(F("HAL TX:"));
    for (size_t i = 0; i < len; i++) {
        Host.print(' ');
        if (data[i] < 0x10)
            Host.print('0');
        Host.print(data[i], HEX);
    }
    Host.println();

    if (len >= 4 && data[0] == HAL_PKT_SYNC) {
        g_lastTxCode = data[1];
        g_lastTxLen = data[2];
        if (g_lastTxLen > HAL_PKT_MAX_PAYLOAD)
            g_lastTxLen = HAL_PKT_MAX_PAYLOAD;
        if (g_lastTxLen > 0)
            memcpy(g_lastTxPayload, data + 3, g_lastTxLen);
        else
            g_lastTxPayload[0] = 0;

        if (g_lastTxCode >= HAL_EVT_WAITING_FINGER && g_lastTxCode <= HAL_EVT_PROCESSING) {
            const char *msg = "?";
            switch (g_lastTxCode) {
            case HAL_EVT_WAITING_FINGER:
                msg = "Scan finger…";
                break;
            case HAL_EVT_FINGER_DETECTED:
                msg = "Finger detected";
                break;
            case HAL_EVT_REMOVE_FINGER:
                msg = "Remove finger";
                break;
            case HAL_EVT_PLACE_AGAIN:
                msg = "Place again";
                break;
            case HAL_EVT_PROCESSING:
                msg = "Processing";
                break;
            default:
                break;
            }
            Host.print(F("  >> "));
            Host.println(msg);
        }
    }
    Host.flush();
}

static void printErr(const char *label, uint8_t code) {
    Host.print(label);
    Host.print(F(" err=0x"));
    if (code < 0x10)
        Host.print('0');
    Host.println(code, HEX);
}

static void printHelp() {
    Host.println(F("DV diag | USART2 115200 ASCII | USART3 AS608 57600"));
    Host.println(F("Sensor: P probe | C count | L list-slots"));
    Host.println(F("HAL FSM: E0 auto-enroll | E5 slot5 | V verify | D5 delete | Q cancel"));
    Host.println(F("HAL inj: Hp STATUS | Hl LIST | Hv VERIFY"));
    Host.println(F("Note: production firmware needs binary HAL (test_hal), not these letters."));
}

static void printHalInjectHint() {
    Host.println(F("HAL inject: Hp=STATUS Hl=LIST Hv=VERIFY (type both letters, then Enter)"));
}

/** True while buffer is an incomplete multi-char command (E0, D5, Hp, …). */
static bool hostNeedsMoreChars(const char *cmd, uint8_t len) {
    if (len != 1)
        return false;
    const char c = cmd[0];
    return c == 'H' || c == 'E' || c == 'e' || c == 'D' || c == 'd';
}

static unsigned long hostIdleTimeoutMs(const char *cmd, uint8_t len) {
    return hostNeedsMoreChars(cmd, len) ? 800UL : 40UL;
}

static bool hostCancelPending() {
    while (Host.available() > 0) {
        char c = (char)Host.read();
        if (c == 'Q' || c == 'q' || c == 0x03)
            return true;
    }
    return false;
}

static void cmdProbe() {
    Host.println(F("AS608: probing…"));
    SensorService *s = sensorInstance();
    if (!sensorEnsureReady(s)) {
        Host.println(F("AS608 FAIL (verifyPassword)"));
        return;
    }
    uint16_t cnt = 0;
    if (sensorTemplateCount(s, &cnt) != FINGERPRINT_OK) {
        Host.println(F("AS608 FAIL getTemplateCount"));
        return;
    }
    Host.print(F("AS608 OK templates="));
    Host.println(cnt);
}

static void cmdCount() {
    SensorService *s = sensorInstance();
    if (!sensorEnsureReady(s)) {
        Host.println(F("sensor not ready"));
        return;
    }
    uint16_t cnt = 0;
    uint8_t p = sensorTemplateCount(s, &cnt);
    if (p != FINGERPRINT_OK) {
        printErr("getTemplateCount", p);
        return;
    }
    Host.print(F("templates="));
    Host.println(cnt);
}

static void cmdListSlots() {
    SensorService *s = sensorInstance();
    if (!sensorEnsureReady(s)) {
        Host.println(F("sensor not ready"));
        return;
    }
    Host.println(F("Scanning slots 1..127 (direct loadModel)…"));
    uint8_t n = 0;
    for (uint8_t id = 1; id <= 127; id++) {
        if ((id & 15) == 1)
            Host.print('.');
        uint8_t p = sensorLoadModel(s, id);
        if (p == FINGERPRINT_OK) {
            Host.println();
            Host.print(F("  slot "));
            Host.println(id);
            n++;
        } else if (sensorIsCommError(p)) {
            Host.println();
            printErr("loadModel", p);
            return;
        }
    }
    Host.println();
    Host.print(F("Total: "));
    Host.println(n);
}

static void injectHal(uint8_t cmd, const uint8_t *payload, uint8_t plen) {
    HalPacket pkt = {};
    pkt.code = cmd;
    pkt.len = plen;
    if (plen > 0 && payload)
        memcpy(pkt.payload, payload, plen);
    opOnHostPacket(&pkt);
}

static bool runFsmUntilIdle(unsigned long timeoutMs) {
    const unsigned long start = millis();
    while (!opIsIdle()) {
        if (millis() - start >= timeoutMs) {
            Host.println(F("FSM timeout"));
            return false;
        }
        if (hostCancelPending()) {
            injectHal(HAL_CMD_CANCEL, nullptr, 0);
            Host.println(F("cancel sent"));
        }
        opTick(millis());
    }
    return true;
}

static void reportLastHalResult(const char *label) {
    Host.print(label);
    Host.print(F(" code=0x"));
    if (g_lastTxCode < 0x10)
        Host.print('0');
    Host.println(g_lastTxCode, HEX);

    if (g_lastTxCode == HAL_RES_SUCCESS && g_lastTxLen >= 1) {
        if (g_lastTxLen == 3) {
            const uint16_t tpl = (uint16_t)(g_lastTxPayload[1] | (g_lastTxPayload[2] << 8));
            Host.print(F("  connected="));
            Host.print(g_lastTxPayload[0]);
            Host.print(F(" templates="));
            Host.println(tpl);
        } else if (g_lastTxLen >= 1) {
            Host.print(F("  payload[0]="));
            Host.println(g_lastTxPayload[0]);
            if (g_lastTxLen > 1) {
                Host.print(F("  ("));
                Host.print(g_lastTxLen);
                Host.println(F(" bytes)"));
            }
        }
    }
}

static void cmdHalPing() {
    g_lastTxCode = 0;
    g_lastTxLen = 0;
    Host.println(F("HAL inject STATUS (0x07)…"));
    injectHal(HAL_CMD_STATUS, nullptr, 0);
    if (opIsIdle())
        reportLastHalResult("Hp");
    else
        Host.println(F("Hp: FSM not idle after STATUS (unexpected)"));
}

static void cmdHalList() {
    g_lastTxCode = 0;
    Host.println(F("HAL inject LIST (0x06)…"));
    injectHal(HAL_CMD_LIST_ENROLLED, nullptr, 0);
    if (!runFsmUntilIdle(65000))
        return;
    reportLastHalResult("Hl");
    if (g_lastTxCode == HAL_RES_SUCCESS && g_lastTxLen >= 1) {
        const uint8_t n = g_lastTxPayload[0];
        Host.print(F("  ids:"));
        for (uint8_t i = 0; i < n && (size_t)(1 + i) < g_lastTxLen; i++) {
            Host.print(' ');
            Host.print(g_lastTxPayload[1 + i]);
        }
        Host.println();
    }
}

static void cmdHalVerify() {
    g_lastTxCode = 0;
    Host.println(F("HAL inject VERIFY (0x02) — place finger within 30s…"));
    injectHal(HAL_CMD_VERIFY_START, nullptr, 0);
    if (!runFsmUntilIdle(35000))
        return;
    reportLastHalResult("Hv");
}

static void cmdEnroll(uint8_t slot) {
    g_lastTxCode = 0;
    Host.print(F("HAL enroll slot request="));
    Host.println(slot);
    const uint8_t pl[1] = {slot};
    injectHal(HAL_CMD_ENROLL_START, pl, 1);
    if (!runFsmUntilIdle(65000))
        return;
    reportLastHalResult("Enroll");
}

static void cmdVerifyFsm() {
    cmdHalVerify();
}

static void cmdDelete(uint8_t slot) {
    g_lastTxCode = 0;
    Host.print(F("HAL delete slot "));
    Host.println(slot);
    const uint8_t pl[1] = {slot};
    injectHal(HAL_CMD_DELETE, pl, 1);
    if (!runFsmUntilIdle(10000))
        return;
    reportLastHalResult("Delete");
}

static char hostBuf[16];
static uint8_t hostLen = 0;
static unsigned long hostLastRxMs = 0;

/** Trim whitespace; keep printable ASCII only. Returns new length (may be 0). */
static uint8_t normalizeHostLine(char *buf, uint8_t len) {
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t'))
        len--;
    uint8_t start = 0;
    while (start < len && (buf[start] == ' ' || buf[start] == '\t'))
        start++;
    if (start > 0) {
        len = (uint8_t)(len - start);
        memmove(buf, buf + start, len);
    }
    uint8_t w = 0;
    for (uint8_t i = 0; i < len; i++) {
        const unsigned char ch = (unsigned char)buf[i];
        if (ch >= 0x20 && ch <= 0x7e)
            buf[w++] = (char)ch;
    }
    buf[w] = '\0';
    return w;
}

static void dispatchHostCommand(const char *cmd, uint8_t len);

static void submitHostLine() {
    hostBuf[hostLen] = '\0';
    const uint8_t len = normalizeHostLine(hostBuf, hostLen);
    hostLen = 0;
    if (len == 0)
        return;
    Host.print(F("> "));
    Host.println(hostBuf);
    dispatchHostCommand(hostBuf, len);
}

static uint8_t parseSlotDigits(const char *s, uint8_t len, uint8_t defaultVal) {
    uint8_t id = 0;
    for (uint8_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)s[i]))
            return defaultVal;
        id = (uint8_t)(id * 10 + (uint8_t)(s[i] - '0'));
        if (id > 127)
            return 127;
    }
    return id > 0 ? id : defaultVal;
}

static void dispatchHostCommand(const char *cmd, uint8_t len) {
    if (len == 0)
        return;

    char c = cmd[0];
    if (!opIsIdle()) {
        if (len == 1 && (c == 'Q' || c == 'q')) {
            injectHal(HAL_CMD_CANCEL, nullptr, 0);
            Host.println(F("cancel"));
            return;
        }
        if (len == 1 && (c == '?' || c == 'h')) {
            printHelp();
            return;
        }
        if (len == 1 && (c == 'H')) {
            printHalInjectHint();
            return;
        }
        Host.println(F("BUSY — send Q to cancel"));
        return;
    }

    uint8_t slot = len > 1 ? parseSlotDigits(cmd + 1, len - 1, 1) : 1;

    if (len >= 2 && (c == 'H' || c == 'h')) {
        const char sub = cmd[1];
        switch (sub) {
        case 'p':
        case 'P':
            cmdHalPing();
            return;
        case 'l':
        case 'L':
            cmdHalList();
            return;
        case 'v':
        case 'V':
            cmdHalVerify();
            return;
        default:
            Host.println(F("unknown H* command"));
            return;
        }
    }

    if (len == 1 && c == 'H') {
        printHalInjectHint();
        return;
    }

    switch (c) {
    case '?':
    case 'h':
        printHelp();
        break;
    case 'P':
    case 'p':
        cmdProbe();
        break;
    case 'C':
    case 'c':
        cmdCount();
        break;
    case 'L':
    case 'l':
        cmdListSlots();
        break;
    case 'E':
    case 'e':
        cmdEnroll(slot);
        break;
    case 'V':
    case 'v':
        cmdVerifyFsm();
        break;
    case 'D':
    case 'd':
        cmdDelete(slot);
        break;
    case 'Q':
    case 'q':
        Host.println(F("idle"));
        break;
    default:
        Host.print(F("unknown: "));
        for (uint8_t i = 0; i < len; i++)
            Host.write(cmd[i]);
        Host.print(F(" (len="));
        Host.print(len);
        Host.println(')');
        break;
    }
}

static void pollAsciiHost() {
    while (Host.available() > 0) {
        char c = (char)Host.read();
        hostLastRxMs = millis();

        if (c == '\r')
            continue;

        if (c == '\n') {
            submitHostLine();
            continue;
        }

        if (c == '\b' || c == 0x7f) {
            if (hostLen > 0)
                hostLen--;
            continue;
        }

        if (c == 0x03) {
            hostLen = 0;
            if (!opIsIdle())
                injectHal(HAL_CMD_CANCEL, nullptr, 0);
            Host.println(F("^C"));
            continue;
        }

        if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7e)
            continue;

        if (hostLen < sizeof(hostBuf) - 1) {
            hostBuf[hostLen++] = c;
            Host.write(c);
        } else {
            hostLen = 0;
            Host.println(F("cmd too long"));
        }
    }

    if (hostLen > 0) {
        const unsigned long idleMs = hostIdleTimeoutMs(hostBuf, hostLen);
        if (millis() - hostLastRxMs > idleMs)
            submitHostLine();
    }
}

void setup() {
    Host.begin(115200);
    Sensor.begin(57600);
    delay(300);

    SensorService *sensor = sensorInstance();
    sensorInit(sensor, &Sensor);
    opInit(sensor);

    halHostSetDiagTxHook(diagTxHook);

    Host.println(F("DV diag ready | USART2 115200"));
    if (sensorEnsureReady(sensor))
        Host.println(F("AS608 sensor ready"));
    else
        Host.println(F("AS608 sensor pending — try P"));
    printHelp();
}

void loop() {
    pollAsciiHost();
    if (!opIsIdle())
        opTick(millis());
}
