#include "sensor_service.h"
#include <Adafruit_Fingerprint.h>

struct SensorService {
    Adafruit_Fingerprint *finger;
    HardwareSerial *port;
    bool ready;
    uint8_t commFailures;
};

static SensorService gInstance;

SensorService *sensorInstance(void) { return &gInstance; }

void sensorInit(SensorService *s, HardwareSerial *port) {
    static Adafruit_Fingerprint finger(port);
    s->finger = &finger;
    s->port = port;
    s->ready = false;
    s->commFailures = 0;
}

static bool reinit(SensorService *s) {
    if (s->port) {
        while (s->port->available() > 0)
            (void)s->port->read();
    }
    s->finger->begin(57600);
    delay(50);
    if (!s->finger->verifyPassword()) {
        s->ready = false;
        return false;
    }
    s->ready = true;
    s->commFailures = 0;
    return true;
}

bool sensorEnsureReady(SensorService *s) {
    if (s->ready)
        return true;
    return reinit(s);
}

void sensorMarkFault(SensorService *s) {
    s->ready = false;
    s->commFailures++;
    if (s->commFailures < 3)
        (void)reinit(s);
}

bool sensorIsCommError(uint8_t code) {
    return code == FINGERPRINT_PACKETRECIEVEERR || code == FINGERPRINT_TIMEOUT ||
           code == FINGERPRINT_BADPACKET;
}

bool sensorIsBadImage(uint8_t code) {
    return code == FINGERPRINT_FEATUREFAIL || code == FINGERPRINT_IMAGEMESS ||
           code == FINGERPRINT_IMAGEFAIL || code == FINGERPRINT_INVALIDIMAGE;
}

bool sensorIsNoFinger(uint8_t code) { return code == FINGERPRINT_NOFINGER; }

uint8_t sensorGetImage(SensorService *s) {
    if (!sensorEnsureReady(s))
        return FINGERPRINT_PACKETRECIEVEERR;
    uint8_t p = s->finger->getImage();
    if (sensorIsCommError(p))
        sensorMarkFault(s);
    return p;
}

uint8_t sensorImage2Tz(SensorService *s, uint8_t slot) {
    if (!sensorEnsureReady(s))
        return FINGERPRINT_PACKETRECIEVEERR;
    uint8_t p = s->finger->image2Tz(slot);
    if (sensorIsCommError(p))
        sensorMarkFault(s);
    return p;
}

uint8_t sensorCreateModel(SensorService *s) {
    uint8_t p = s->finger->createModel();
    if (sensorIsCommError(p))
        sensorMarkFault(s);
    return p;
}

uint8_t sensorStoreModel(SensorService *s, uint8_t id) {
    uint8_t p = s->finger->storeModel(id);
    if (sensorIsCommError(p))
        sensorMarkFault(s);
    return p;
}

uint8_t sensorFastSearch(SensorService *s, uint16_t *matchId, uint16_t *score) {
    uint8_t p = s->finger->fingerFastSearch();
    if (p == FINGERPRINT_OK) {
        if (matchId)
            *matchId = s->finger->fingerID;
        if (score)
            *score = s->finger->confidence;
    }
    if (sensorIsCommError(p))
        sensorMarkFault(s);
    return p;
}

uint8_t sensorDeleteModel(SensorService *s, uint8_t id) {
    if (!sensorEnsureReady(s))
        return FINGERPRINT_PACKETRECIEVEERR;
    uint8_t p = s->finger->deleteModel(id);
    if (sensorIsCommError(p))
        sensorMarkFault(s);
    return p;
}

uint8_t sensorLoadModel(SensorService *s, uint8_t id) {
    if (!sensorEnsureReady(s))
        return FINGERPRINT_PACKETRECIEVEERR;
    uint8_t p = s->finger->loadModel(id);
    if (sensorIsCommError(p))
        sensorMarkFault(s);
    return p;
}

uint8_t sensorTemplateCount(SensorService *s, uint16_t *count) {
    if (!sensorEnsureReady(s))
        return FINGERPRINT_PACKETRECIEVEERR;
    uint8_t p = s->finger->getTemplateCount();
    if (p == FINGERPRINT_OK && count)
        *count = s->finger->templateCount;
    if (sensorIsCommError(p))
        sensorMarkFault(s);
    return p;
}

