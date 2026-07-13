#pragma once

// Web UI in modalità standalone (SoftAP "RP_Handle"): stessa pagina del telaio
class WebUIStandalone {
public:
    void begin();
    void handleClients();
    void sendTelemetryBuffer(float* readyBuffer);
    void sendMetrics();
};

extern WebUIStandalone webUIStandalone;
