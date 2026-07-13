#pragma once

// Server HTTP + WebSocket: pagina UI, telemetria realtime, comandi e OTA via /update
class WebUI {
public:
    void begin();
    void handleClients();                        // cleanup ws + esito scansione WiFi
    void sendTelemetryBuffer(float* readyBuffer);
    void sendMetrics();
};

extern WebUI webApp;
