#include "Telemetry.h"
#include <esp_now.h>
#include "SharedData.h"

// Callback interna di ESP-NOW
void onEspNowRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
    struct { int32_t t; uint16_t d; uint8_t hr; } __attribute__((packed)) payload;
    memcpy(&payload, data, sizeof(payload));
    
    telemetry.encoderTicks = payload.t;
    telemetry.seatPositionMeters = payload.d / 1000.0;
    telemetry.heartRate = payload.hr;
    telemetry.lastPacketTime = millis();
    telemetry.active = true;
}

void TelemetryRadio::begin() {
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(onEspNowRecv);
    }
}

void TelemetryRadio::checkTimeout() {
    if (millis() - telemetry.lastPacketTime > 500) {
        telemetry.active = false;
    }
}