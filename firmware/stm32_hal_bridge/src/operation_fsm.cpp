#include "operation_fsm.h"
#include "hal_host.h"
#include <Adafruit_Fingerprint.h>
#include <string.h>

static SensorService *gSensor = nullptr;

enum class Phase : uint8_t {
    Idle,
    EnrollWait1,
    EnrollSettle1,
    EnrollCapture1,
    EnrollWaitLift,
    EnrollPause,
    EnrollWait2,
    EnrollSettle2,
    EnrollCapture2,
    EnrollFinish,
    VerifyWait,
    VerifySettle,
    VerifyCapture,
    VerifySearch,
    ListScan,
    DeleteSync,
};

static Phase phase = Phase::Idle;
static uint8_t enrollSlot = 1;
static uint8_t enrollRetries = 0;
static uint8_t listIds[127];
static uint8_t listCount = 0;
static uint8_t listScanId = 1;
static uint8_t pendingDeleteId = 0;
static uint16_t matchedId = 0;
static unsigned long pauseUntilMs = 0;
static unsigned long opDeadlineMs = 0;
static unsigned long keyWindowUntilMs = 0;
static bool cancelRequested = false;
static bool waitingFingerEvtSent = false;
static uint8_t enrollAfterLift = 1;
static uint8_t vaultKey[HAL_KEY_SIZE];

static const uint8_t kSettleMs = 200;
static const uint8_t kMaxEnrollRetries = 5;
static const unsigned long kVerifyTimeoutMs = 30000;
static const unsigned long kEnrollTimeoutMs = 60000;
static const unsigned long kListTimeoutMs = 60000;
static const unsigned long kDeleteTimeoutMs = 10000;
static const unsigned long kKeyWindowMs = 10000;
static const uint16_t kMaxTemplates = 128; /* AS608 library capacity */

static void deriveVaultKey() {
    uint32_t uid[3];
    memcpy(uid, (const void *)0x1FFFF7E8, 12);
    for (int i = 0; i < HAL_KEY_SIZE; i++) {
        uint8_t b = (uint8_t)(uid[i % 3] >> ((i % 4) * 8));
        vaultKey[i] = (uint8_t)(b ^ (uint8_t)(0xA5 + i));
    }
}

static void goIdle() {
    phase = Phase::Idle;
    pauseUntilMs = 0;
    opDeadlineMs = 0;
    cancelRequested = false;
    waitingFingerEvtSent = false;
    enrollRetries = 0;
}

static void failResult(uint8_t code, const uint8_t *payload, uint8_t plen) {
    halHostSendResult(code, payload, plen);
    goIdle();
}

static void failSensorComm() { failResult(HAL_RES_SENSOR_ERROR, nullptr, 0); }

static void failTimeout() { failResult(HAL_RES_TIMEOUT, nullptr, 0); }

static bool opTimedOut(unsigned long nowMs) {
    return opDeadlineMs != 0 && nowMs >= opDeadlineMs;
}

static void startOpDeadline(unsigned long nowMs, unsigned long timeoutMs) {
    opDeadlineMs = nowMs + timeoutMs;
}

static bool retryBadCapture(uint8_t code, uint8_t buffer) {
    if (!sensorIsBadImage(code))
        return false;
    enrollRetries++;
    if (enrollRetries >= kMaxEnrollRetries) {
        failResult(HAL_RES_FAIL_BAD_IMAGE, nullptr, 0);
        return true;
    }
    halHostSendEvent(HAL_EVT_REMOVE_FINGER);
    pauseUntilMs = 0;
    enrollAfterLift = buffer;
    phase = Phase::EnrollWaitLift;
    return true;
}

static void handleCancel() {
    if (phase == Phase::Idle) {
        uint8_t ok = 0;
        halHostSendResult(HAL_RES_SUCCESS, &ok, 0);
        return;
    }
    cancelRequested = true;
}

static void handleGetKey(unsigned long nowMs) {
    if (nowMs > keyWindowUntilMs) {
        failResult(HAL_RES_FAIL_NO_MATCH, nullptr, 0);
        return;
    }
    halHostSendResult(HAL_RES_KEY_DATA, vaultKey, HAL_KEY_SIZE);
    keyWindowUntilMs = 0;
}

static void handleDelete(const HalPacket *pkt) {
    if (pkt->len < 1 || pkt->payload[0] < 1 || pkt->payload[0] > 127) {
        failResult(HAL_RES_FAIL_INVALID_ID, nullptr, 0);
        return;
    }
    pendingDeleteId = pkt->payload[0];
    phase = Phase::DeleteSync;
    startOpDeadline(millis(), kDeleteTimeoutMs);
}

static void handleList() {
    listCount = 0;
    listScanId = 1;
    pauseUntilMs = 0;
    phase = Phase::ListScan;
    startOpDeadline(millis(), kListTimeoutMs);
}

enum class ResolveSlot : uint8_t { Ok, Full, CommError };

/** requested 0 = first free slot 1..127 */
static ResolveSlot resolveEnrollSlot(uint8_t requested, uint8_t *outSlot) {
    if (requested >= 1 && requested <= 127) {
        *outSlot = requested;
        return ResolveSlot::Ok;
    }
    if (requested != 0)
        return ResolveSlot::Full;
    for (uint8_t s = 1; s <= 127; s++) {
        uint8_t p = sensorLoadModel(gSensor, s);
        if (p == FINGERPRINT_OK)
            continue;
        if (sensorIsCommError(p))
            return ResolveSlot::CommError;
        *outSlot = s;
        return ResolveSlot::Ok;
    }
    return ResolveSlot::Full;
}

static void startEnroll(uint8_t requestedId, unsigned long nowMs) {
    uint8_t id = 0;
    const ResolveSlot rs = resolveEnrollSlot(requestedId, &id);
    if (rs == ResolveSlot::CommError) {
        failSensorComm();
        return;
    }
    if (rs != ResolveSlot::Ok) {
        if (requestedId == 0)
            failResult(HAL_RES_FAIL_SLOT_FULL, nullptr, 0);
        else
            failResult(HAL_RES_FAIL_INVALID_ID, nullptr, 0);
        return;
    }
    if (sensorLoadModel(gSensor, id) == FINGERPRINT_OK) {
        uint8_t pl[1] = {id};
        failResult(HAL_RES_FAIL_ALREADY_EXISTS, pl, 1);
        return;
    }
    uint16_t cnt = 0;
    if (sensorTemplateCount(gSensor, &cnt) == FINGERPRINT_OK && cnt >= kMaxTemplates) {
        failResult(HAL_RES_FAIL_SLOT_FULL, nullptr, 0);
        return;
    }
    enrollSlot = id;
    enrollRetries = 0;
    waitingFingerEvtSent = false;
    startOpDeadline(nowMs, kEnrollTimeoutMs);
    phase = Phase::EnrollWait1;
    halHostSendEvent(HAL_EVT_WAITING_FINGER);
    waitingFingerEvtSent = true;
}

static void startVerify(unsigned long nowMs) {
    waitingFingerEvtSent = false;
    startOpDeadline(nowMs, kVerifyTimeoutMs);
    phase = Phase::VerifyWait;
    halHostSendEvent(HAL_EVT_WAITING_FINGER);
    waitingFingerEvtSent = true;
}

void opInit(SensorService *sensor) {
    gSensor = sensor;
    deriveVaultKey();
}

void opOnHostPacket(const HalPacket *pkt) {
    const unsigned long nowMs = millis();

    if (phase != Phase::Idle) {
        if (pkt->code == HAL_CMD_CANCEL) {
            handleCancel();
            return;
        }
        if (pkt->code != HAL_CMD_GET_KEY) {
            halHostSendResult(HAL_RES_BUSY, nullptr, 0);
            return;
        }
    }

    switch (pkt->code) {
    case HAL_CMD_CANCEL:
        handleCancel();
        break;
    case HAL_CMD_GET_KEY:
        handleGetKey(nowMs);
        break;
    case HAL_CMD_DELETE:
        handleDelete(pkt);
        break;
    case HAL_CMD_LIST_ENROLLED:
        handleList();
        break;
    case HAL_CMD_ENROLL_START:
        if (pkt->len < 1 || pkt->payload[0] > 127) {
            failResult(HAL_RES_FAIL_INVALID_ID, nullptr, 0);
            break;
        }
        startEnroll(pkt->payload[0], nowMs);
        break;
    case HAL_CMD_VERIFY_START:
        startVerify(nowMs);
        break;
    case HAL_CMD_STATUS: {
        uint8_t body[3] = {0, 0, 0};
        if (sensorEnsureReady(gSensor)) {
            body[0] = 1;
            uint16_t cnt = 0;
            if (sensorTemplateCount(gSensor, &cnt) == FINGERPRINT_OK) {
                body[1] = (uint8_t)(cnt > 255 ? 255 : cnt);
                body[2] = (uint8_t)(cnt >> 8);
            }
        }
        halHostSendResult(HAL_RES_SUCCESS, body, 3);
        break;
    }
    default:
        failResult(HAL_RES_FAIL_INVALID_ID, nullptr, 0);
        break;
    }
}

void opTick(unsigned long nowMs) {
    if (cancelRequested && phase != Phase::Idle) {
        failResult(HAL_RES_CANCELLED, nullptr, 0);
        return;
    }
    if (phase != Phase::Idle && opTimedOut(nowMs)) {
        failTimeout();
        return;
    }

    switch (phase) {
    case Phase::Idle:
        break;

    case Phase::DeleteSync: {
        uint8_t lp = sensorLoadModel(gSensor, pendingDeleteId);
        if (sensorIsCommError(lp)) {
            failSensorComm();
            break;
        }
        if (lp != FINGERPRINT_OK) {
            failResult(HAL_RES_FAIL_NO_MATCH, nullptr, 0);
            break;
        }
        uint8_t p = sensorDeleteModel(gSensor, pendingDeleteId);
        if (sensorIsCommError(p)) {
            failSensorComm();
            break;
        }
        if (p != FINGERPRINT_OK) {
            failResult(HAL_RES_FAIL_NO_MATCH, nullptr, 0);
            break;
        }
        uint8_t pl[1] = {pendingDeleteId};
        halHostSendResult(HAL_RES_SUCCESS, pl, 1);
        goIdle();
        break;
    }

    case Phase::ListScan: {
        if (pauseUntilMs != 0 && nowMs < pauseUntilMs)
            return;

        if (listScanId > 127) {
            /* HAL max payload 64 B → at most 63 ids in one LIST response */
            const uint8_t sendCount = (listCount > 63) ? 63 : listCount;
            uint8_t body[64];
            body[0] = sendCount;
            for (uint8_t i = 0; i < sendCount; i++)
                body[1 + i] = listIds[i];
            halHostSendResult(HAL_RES_SUCCESS, body, (uint8_t)(1 + sendCount));
            goIdle();
            break;
        }
        uint8_t p = sensorLoadModel(gSensor, listScanId);
        if (p == FINGERPRINT_OK && listCount < 127)
            listIds[listCount++] = listScanId;
        else if (sensorIsCommError(p)) {
            failSensorComm();
            break;
        }
        listScanId++;
        pauseUntilMs = nowMs + 10; // 10ms pacing delay between slot queries
        break;
    }

    case Phase::EnrollWait1: {
        uint8_t p = sensorGetImage(gSensor);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (sensorIsNoFinger(p))
            return;
        if (p != FINGERPRINT_OK) {
            // Bad image during wait: Keep waiting/polling until deadline
            return;
        }
        halHostSendEvent(HAL_EVT_FINGER_DETECTED);
        pauseUntilMs = nowMs + kSettleMs;
        phase = Phase::EnrollSettle1;
        break;
    }

    case Phase::EnrollSettle1:
        if (nowMs < pauseUntilMs)
            return;
        phase = Phase::EnrollCapture1;
        break;

    case Phase::EnrollCapture1: {
        uint8_t p = sensorImage2Tz(gSensor, 1);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (p != FINGERPRINT_OK) {
            if (retryBadCapture(p, 1))
                return;
            failResult(HAL_RES_FAIL_BAD_IMAGE, nullptr, 0);
            return;
        }
        enrollAfterLift = 2;
        halHostSendEvent(HAL_EVT_REMOVE_FINGER);
        phase = Phase::EnrollWaitLift;
        break;
    }

    case Phase::EnrollWaitLift: {
        uint8_t p = sensorGetImage(gSensor);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (!sensorIsNoFinger(p))
            return;
        pauseUntilMs = nowMs + 800;
        phase = Phase::EnrollPause;
        break;
    }

    case Phase::EnrollPause:
        if (nowMs < pauseUntilMs)
            return;
        halHostSendEvent(HAL_EVT_PLACE_AGAIN);
        phase = (enrollAfterLift == 1) ? Phase::EnrollWait1 : Phase::EnrollWait2;
        break;

    case Phase::EnrollWait2: {
        uint8_t p = sensorGetImage(gSensor);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (sensorIsNoFinger(p))
            return;
        if (p != FINGERPRINT_OK) {
            // Bad image during wait: Keep waiting/polling until deadline
            return;
        }
        halHostSendEvent(HAL_EVT_FINGER_DETECTED);
        pauseUntilMs = nowMs + kSettleMs;
        phase = Phase::EnrollSettle2;
        break;
    }

    case Phase::EnrollSettle2:
        if (nowMs < pauseUntilMs)
            return;
        phase = Phase::EnrollCapture2;
        break;

    case Phase::EnrollCapture2: {
        uint8_t p = sensorImage2Tz(gSensor, 2);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (p != FINGERPRINT_OK) {
            if (retryBadCapture(p, 2))
                return;
            failResult(HAL_RES_FAIL_BAD_IMAGE, nullptr, 0);
            return;
        }
        halHostSendEvent(HAL_EVT_PROCESSING);
        phase = Phase::EnrollFinish;
        break;
    }

    case Phase::EnrollFinish: {
        uint8_t p = sensorCreateModel(gSensor);
        if (p == FINGERPRINT_ENROLLMISMATCH) {
            halHostSendEvent(HAL_EVT_REMOVE_FINGER);
            enrollRetries++;
            if (enrollRetries >= kMaxEnrollRetries) {
                failResult(HAL_RES_FAIL_BAD_IMAGE, nullptr, 0);
                return;
            }
            halHostSendEvent(HAL_EVT_PLACE_AGAIN);
            phase = Phase::EnrollWait2;
            return;
        }
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (p != FINGERPRINT_OK) {
            failResult(HAL_RES_FAIL_BAD_IMAGE, nullptr, 0);
            return;
        }
        p = sensorStoreModel(gSensor, enrollSlot);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (p != FINGERPRINT_OK) {
            failResult(HAL_RES_FAIL_BAD_IMAGE, nullptr, 0);
            return;
        }
        uint8_t pl[1] = {enrollSlot};
        halHostSendResult(HAL_RES_SUCCESS, pl, 1);
        goIdle();
        break;
    }

    case Phase::VerifyWait: {
        uint8_t p = sensorGetImage(gSensor);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (sensorIsNoFinger(p))
            return;
        if (p != FINGERPRINT_OK) {
            // Bad image during wait: Keep waiting/polling until deadline
            return;
        }
        halHostSendEvent(HAL_EVT_FINGER_DETECTED);
        pauseUntilMs = nowMs + kSettleMs;
        phase = Phase::VerifySettle;
        break;
    }

    case Phase::VerifySettle:
        if (nowMs < pauseUntilMs)
            return;
        phase = Phase::VerifyCapture;
        break;

    case Phase::VerifyCapture: {
        uint8_t p = sensorImage2Tz(gSensor, 1);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (p != FINGERPRINT_OK) {
            failResult(HAL_RES_FAIL_BAD_IMAGE, nullptr, 0);
            return;
        }
        halHostSendEvent(HAL_EVT_PROCESSING);
        phase = Phase::VerifySearch;
        break;
    }

    case Phase::VerifySearch: {
        uint16_t mid = 0, score = 0;
        uint8_t p = sensorFastSearch(gSensor, &mid, &score);
        if (sensorIsCommError(p)) {
            failSensorComm();
            return;
        }
        if (p != FINGERPRINT_OK) {
            failResult(HAL_RES_FAIL_NO_MATCH, nullptr, 0);
            return;
        }
        keyWindowUntilMs = nowMs + kKeyWindowMs;
        uint8_t pl[1] = {(uint8_t)mid};
        halHostSendResult(HAL_RES_SUCCESS, pl, 1);
        goIdle();
        break;
    }
    }
}

bool opIsIdle(void) {
    return phase == Phase::Idle;
}
