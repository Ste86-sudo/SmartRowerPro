#pragma once
#include <stdint.h>

// Link ESP-NOW verso la maniglia: ricezione forza, heartbeat e comandi calibrazione
class TelemetryRadio {
public:
    static void begin();
    static void checkTimeout();
    static void sendHeartbeat();
    static void sendTare();
    static void sendCalib(float refWeightKg);
    static void sendSetTare(int32_t t);
    static void sendSetScale(float s);
};
