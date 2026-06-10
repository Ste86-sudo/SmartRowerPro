#pragma once
#include <Arduino.h>

class WebUIStandalone {
public:
    void begin();
    void handleClients();
    void sendTelemetryBuffer(float* readyBuffer);
    void sendMetrics();
};

extern WebUIStandalone webUIStandalone;
