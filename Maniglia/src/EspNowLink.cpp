#include "EspNowLink.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

EspNowLink espNowLink;

static ForcePacket txPacket;
static uint8_t pairedMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool paired = false;
static bool hbReceived = false;
static esp_now_peer_info_t peerInfo;

static volatile uint8_t pendingCmd = 0;
static volatile float pendingParam = 0;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    const uint8_t* mac = info->src_addr;
#else
static void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (len == 1 && data[0] == PKT_HEARTBEAT) {
        hbReceived = true;
        if (!paired) {
            paired = true;
            memcpy(pairedMac, mac, 6);
            memcpy(peerInfo.peer_addr, pairedMac, 6);
            esp_now_add_peer(&peerInfo);
            Serial.printf("[ESP-NOW] Pairing con Telaio: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    } else if (len == (int)sizeof(CmdPacket) && data[0] == PKT_MAGIC_CMD) {
        if (paired && memcmp(mac, pairedMac, 6) != 0) return;   // ignora terzi
        CmdPacket pkt;
        memcpy(&pkt, data, sizeof(pkt));
        float param;
        memcpy(&param, pkt.param, 4);
        pendingParam = param;
        pendingCmd = pkt.cmd;
    }
}

bool EspNowLink::begin() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Errore inizializzazione");
        return false;
    }
    esp_now_register_recv_cb(onEspNowRecv);

    lockChannel();

    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, pairedMac, 6);
    peerInfo.channel = RP_CHANNEL;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESP-NOW] Errore aggiunta peer broadcast");
        return false;
    }

    txPacket.magic = PKT_MAGIC_FORCE;
    txPacket.seq = 0;
    txPacket.flags = 0;
    return true;
}

void EspNowLink::lockChannel() {
    esp_wifi_set_channel(RP_CHANNEL, WIFI_SECOND_CHAN_NONE);
}

bool EspNowLink::heartbeatReceived() const { return hbReceived; }
bool EspNowLink::isPaired() const { return paired; }

bool EspNowLink::fetchCommand(uint8_t& cmd, float& param) {
    if (pendingCmd == 0) return false;
    cmd = pendingCmd;
    param = pendingParam;
    pendingCmd = 0;
    return true;
}

void EspNowLink::sendForce(int32_t rawAdc, int16_t forceCenti, int16_t peakCenti) {
    txPacket.rawAdc = rawAdc;
    txPacket.forceCenti = forceCenti;
    txPacket.peakCenti = peakCenti;

    esp_err_t res = esp_now_send(pairedMac, (uint8_t*)&txPacket, sizeof(ForcePacket));
    if (res != ESP_OK) {
        // Rate-limit del log errori: a 100 Hz floodava la seriale
        static unsigned long lastErrLog = 0;
        if (millis() - lastErrLog > 1000) {
            lastErrLog = millis();
            Serial.printf("[ESP-NOW] send err: %d\n", res);
        }
    }
    txPacket.seq++;
}
