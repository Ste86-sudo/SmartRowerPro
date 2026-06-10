#pragma once
#include <stdint.h>

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