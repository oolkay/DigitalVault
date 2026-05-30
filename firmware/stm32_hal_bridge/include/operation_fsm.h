#pragma once

#include "hal_proto.h"
#include "sensor_service.h"

void opInit(SensorService *sensor);
void opOnHostPacket(const HalPacket *pkt);
void opTick(unsigned long nowMs);
bool opIsIdle(void);
