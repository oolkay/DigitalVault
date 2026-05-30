/**
 * DigitalVault STM32 HAL bridge — USART2 binary HAL @ 115200, AS608 @ USART3 57600.
 */
#include "hal_host.h"
#include "operation_fsm.h"
#include "sensor_service.h"
#include <Arduino.h>

static HardwareSerial Host(PA3, PA2);
static HardwareSerial Sensor(PB11, PB10);
HardwareSerial &hostSerial() { return Host; }

static void onPacket(const HalPacket *pkt) { opOnHostPacket(pkt); }

void setup() {
    Host.begin(115200);
    Sensor.begin(57600);
    delay(300);
    SensorService *sensor = sensorInstance();
    sensorInit(sensor, &Sensor);
    opInit(sensor);
    if (sensorEnsureReady(sensor))
        Host.println("DV HAL bridge ready");
    else
        Host.println("DV HAL bridge ready (sensor pending)");
}

void loop() {
    const unsigned long nowMs = millis();
    halHostPollRx(onPacket);
    opTick(nowMs);
}
