#include "Telemetry.h"
#include "SharedData.h"
#include <esp_now.h>
#include <Arduino.h>
#include "RowerProtocol.h"

static uint8_t targetMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool isPaired = false;
static esp_now_peer_info_t peerInfo;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const uint8_t* mac = info->src_addr;
#else
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (len < (int)sizeof(ForcePacket)) return;
    
    ForcePacket packet;
    memcpy((void*)&packet, data, sizeof(ForcePacket));
    
    if (packet.magic != PKT_MAGIC_FORCE) return;

    if (!isPaired) {
        isPaired = true;
        memcpy(targetMac, mac, 6);
        memcpy(peerInfo.peer_addr, targetMac, 6);
        peerInfo.channel = RP_CHANNEL;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
        Serial.printf("[ESPNOW] Pairing con Maniglia: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    portENTER_CRITICAL(&telemetryMux);
    if (telemetry.lastSeq > 0) {
        if (packet.seq > telemetry.lastSeq + 1) {
            telemetry.droppedPackets += (packet.seq - telemetry.lastSeq - 1);
        }
    }
    telemetry.lastSeq = packet.seq;
    
    telemetry.currentForce   = packet.forceCenti / 100.0f;
    telemetry.remotePeakForce = packet.peakCenti / 100.0f;
    metrics.rawAdc           = packet.rawAdc;
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
    
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, targetMac, 6);
    peerInfo.channel = RP_CHANNEL; 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    
    Serial.println("[ESPNOW] In ascolto dalla Maniglia (e peer broadcast aggiunto)");
}

void TelemetryRadio::sendHeartbeat() {
    static unsigned long lastHb = 0;
    if (millis() - lastHb >= 500) {
        lastHb = millis();
        uint8_t hb = PKT_HEARTBEAT;
        esp_now_send(targetMac, &hb, 1);
    }
}

void TelemetryRadio::sendTare() {
    uint8_t buf[6];
    buf[0] = PKT_MAGIC_CMD;
    buf[1] = CMD_TARE;
    float param = 0.0f;
    memcpy(&buf[2], &param, 4);
    esp_now_send(targetMac, buf, 6);
}

void TelemetryRadio::sendCalib(float refWeight) {
    uint8_t buf[6];
    buf[0] = PKT_MAGIC_CMD;
    buf[1] = CMD_CALIB;
    memcpy(&buf[2], &refWeight, 4);
    esp_now_send(targetMac, buf, 6);
}

void TelemetryRadio::checkTimeout() {
    if (telemetry.active && (millis() - telemetry.lastPacketTime > 500)) {
        telemetry.active = false;
        Serial.println("[ESPNOW] Timeout — Maniglia non risponde");
    }
}
