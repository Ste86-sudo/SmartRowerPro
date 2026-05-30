#pragma once
#include <ESPAsyncWebServer.h>

class WebUI {
public:
    void begin();
    void handleClients();
    void sendTelemetryBuffer(volatile float* readyBuffer);
    void sendMetrics();
};

extern WebUI webApp;