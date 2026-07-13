#include "Telemetry.h"
#include "SharedData.h"
#include <esp_now.h>
#include <Arduino.h>
#include "RowerProtocol.h"

static uint8_t targetMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool isPaired = false;
static esp_now_peer_info_t peerInfo;

static void sendCmd(const CmdPacket& pkt) {
    esp_now_send(targetMac, (const uint8_t*)&pkt, sizeof(pkt));
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const uint8_t* mac = info->src_addr;
#else
static void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (len < (int)sizeof(ForcePacket)) return;

    ForcePacket packet;
    memcpy(&packet, data, sizeof(ForcePacket));

    if (packet.magic != PKT_MAGIC_FORCE) return;

    // Pairing: aggancia il MAC della prima maniglia che trasmette
    if (!isPaired) {
        isPaired = true;
        memcpy(targetMac, mac, 6);
        memcpy(peerInfo.peer_addr, targetMac, 6);
        peerInfo.channel = RP_CHANNEL;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
        Serial.printf("[ESPNOW] Pairing con Maniglia: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    portENTER_CRITICAL(&telemetryMux);
    if (telemetry.lastSeq > 0 && packet.seq > telemetry.lastSeq + 1) {
        telemetry.droppedPackets += (packet.seq - telemetry.lastSeq - 1);
    }
    telemetry.lastSeq = packet.seq;

    telemetry.currentForce = packet.forceCenti / 100.0f;
    telemetry.remotePeakForce = packet.peakCenti / 100.0f;
    metrics.rawAdc = packet.rawAdc;
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

    // Peer broadcast iniziale: sostituito dal MAC reale al pairing
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, targetMac, 6);
    peerInfo.channel = RP_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    Serial.println("[ESPNOW] In ascolto dalla Maniglia (peer broadcast attivo)");
}

void TelemetryRadio::sendHeartbeat() {
    static unsigned long lastHb = 0;
    if (millis() - lastHb >= 500) {
        lastHb = millis();
        uint8_t hb = PKT_HEARTBEAT;
        esp_now_send(targetMac, &hb, 1);
    }
}

void TelemetryRadio::sendTare()             { sendCmd(makeCmdPacket(CMD_TARE, 0.0f)); }
void TelemetryRadio::sendCalib(float refKg) { sendCmd(makeCmdPacket(CMD_CALIB, refKg)); }
void TelemetryRadio::sendSetTare(int32_t t) { sendCmd(makeCmdPacket(CMD_SET_TARE, t)); }
void TelemetryRadio::sendSetScale(float s)  { sendCmd(makeCmdPacket(CMD_SET_SCALE, s)); }

void TelemetryRadio::checkTimeout() {
    portENTER_CRITICAL(&telemetryMux);
    bool active = telemetry.active;
    unsigned long lastTime = telemetry.lastPacketTime;
    portEXIT_CRITICAL(&telemetryMux);

    if (active && (millis() - lastTime > 500)) {
        portENTER_CRITICAL(&telemetryMux);
        telemetry.active = false;
        portEXIT_CRITICAL(&telemetryMux);
        Serial.println("[ESPNOW] Timeout — Maniglia non risponde");
    }
}
