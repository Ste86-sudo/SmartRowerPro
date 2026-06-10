#include "Telemetry.h"
#include "SharedData.h"
#include <esp_now.h>
#include <Arduino.h>

// Pacchetto FORZA (maniglia -> telaio), 100 Hz
typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint32_t seq;
    int32_t  rawAdc;
    int16_t  forceCenti;
    int16_t  peakCenti;
    uint8_t  flags;
} ForcePacket;

static uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (len < (int)sizeof(ForcePacket)) return;
    
    ForcePacket packet;
    memcpy((void*)&packet, data, sizeof(ForcePacket));
    
    if (packet.magic != 0xA1) return;

    portENTER_CRITICAL(&telemetryMux);
    telemetry.currentForce   = packet.forceCenti / 100.0f;
    telemetry.remotePeakForce = packet.peakCenti / 100.0f;   // brief Fase 2: salva il picco ricevuto
    metrics.rawAdc           = packet.rawAdc;                // brief Fase 2: ADC grezzo per schermata calibrazione
    telemetry.lastSeq        = packet.seq;
    telemetry.lastPacketTime = millis();
    telemetry.active = true;
    portEXIT_CRITICAL(&telemetryMux);
}

void TelemetryRadio::begin() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Init fallita");
        return;
    }
    esp_now_register_recv_cb(onEspNowRecv);
    
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 0; 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    
    Serial.println("[ESPNOW] In ascolto dalla Maniglia (e peer broadcast aggiunto)");
}

void TelemetryRadio::sendHeartbeat() {
    static unsigned long lastHb = 0;
    if (millis() - lastHb >= 500) {
        lastHb = millis();
        uint8_t hb = 0x48; // 'H'
        esp_now_send(broadcastMac, &hb, 1);
    }
}

void TelemetryRadio::sendTare() {
    uint8_t buf[6];
    buf[0] = 0xC1;
    buf[1] = 1; // TARE
    float param = 0.0f;
    memcpy(&buf[2], &param, 4);
    esp_now_send(broadcastMac, buf, 6);
}

void TelemetryRadio::sendCalib(float refWeight) {
    uint8_t buf[6];
    buf[0] = 0xC1;
    buf[1] = 2; // CALIB
    memcpy(&buf[2], &refWeight, 4);
    esp_now_send(broadcastMac, buf, 6);
}

void TelemetryRadio::checkTimeout() {
    if (telemetry.active && (millis() - telemetry.lastPacketTime > 500)) {
        telemetry.active = false;
        Serial.println("[ESPNOW] Timeout — Maniglia non risponde");
    }
}
