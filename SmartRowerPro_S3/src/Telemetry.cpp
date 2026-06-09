#include "Telemetry.h"
#include "SharedData.h"
#include <esp_now.h>
#include <Arduino.h>

static struct __attribute__((packed)) {
    int32_t t;
    uint16_t d;
    uint8_t  hr;
} espNowPayload;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (len < (int)sizeof(espNowPayload)) return;
    memcpy((void*)&espNowPayload, data, sizeof(espNowPayload));

    telemetry.encoderTicks       = espNowPayload.t;
    telemetry.seatPositionMeters = espNowPayload.d / 1000.0f;
    telemetry.heartRate          = espNowPayload.hr;
    telemetry.lastPacketTime     = millis();
    telemetry.active             = true;
}

void TelemetryRadio::begin() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Init fallita");
        return;
    }
    esp_now_register_recv_cb(onEspNowRecv);
    Serial.println("[ESPNOW] In ascolto dal C3");
}

void TelemetryRadio::checkTimeout() {
    if (telemetry.active && (millis() - telemetry.lastPacketTime > 500)) {
        telemetry.active = false;
        Serial.println("[ESPNOW] Timeout — C3 non risponde");
    }
}
