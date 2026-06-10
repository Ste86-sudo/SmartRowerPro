#pragma once

class TelemetryRadio {
public:
    static void begin();
    static void checkTimeout();
    static void sendHeartbeat();
    static void sendTare();
    static void sendCalib(float refWeight);
};