#pragma once

#include <Arduino.h>
#include <stdint.h>

struct SensorService;

SensorService *sensorInstance(void);
void sensorInit(SensorService *s, HardwareSerial *port);
bool sensorEnsureReady(SensorService *s);
void sensorMarkFault(SensorService *s);

uint8_t sensorGetImage(SensorService *s);
uint8_t sensorImage2Tz(SensorService *s, uint8_t slot);
uint8_t sensorCreateModel(SensorService *s);
uint8_t sensorStoreModel(SensorService *s, uint8_t id);
uint8_t sensorFastSearch(SensorService *s, uint16_t *matchId, uint16_t *score);
uint8_t sensorDeleteModel(SensorService *s, uint8_t id);
uint8_t sensorLoadModel(SensorService *s, uint8_t id);
uint8_t sensorTemplateCount(SensorService *s, uint16_t *count);

bool sensorIsCommError(uint8_t code);
bool sensorIsBadImage(uint8_t code);
bool sensorIsNoFinger(uint8_t code);
