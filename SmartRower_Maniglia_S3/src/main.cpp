#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "Config.h"
#include "LoadCell.h"
#include "CalibStore.h"
#include "PhysicsLite.h"
#include "WebUIStandalone.h"

// Pacchetto FORZA (maniglia -> telaio), 100 Hz
typedef struct __attribute__((packed)) {
    uint8_t  magic;       
    uint32_t seq;         
    int32_t  rawAdc;      
    int16_t  forceCenti;  
    int16_t  peakCenti;   
    uint8_t  flags;       
} ForcePacket;

ForcePacket currentPacket;
int16_t currentPeakCenti = 0;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

bool standaloneMode = false;
bool heartbeatReceived = false;
unsigned long bootTime = 0;
bool modeSelected = false;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (len == 1 && data[0] == 0x48) {
        heartbeatReceived = true;
    } else if (len == 6 && data[0] == 0xC1) {
        uint8_t cmd = data[1];
        float param;
        memcpy(&param, &data[2], 4);
        if (cmd == 1) { // TARE
            calibStore.saveCalibration(loadCell.getLastRaw(), calibStore.getScale());
            Serial.println("[ESP-NOW] Ricevuto TARE da Telaio");
        } else if (cmd == 2) { // CALIB
            if (param > 0.01f) {
                float delta = (float)loadCell.getLastRaw() - (float)calibStore.getTare();
                float newScale = delta / param;
                if (fabsf(newScale) > 0.001f) {
                    calibStore.saveCalibration(calibStore.getTare(), newScale);
                    Serial.println("[ESP-NOW] Ricevuto CALIB da Telaio");
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[MANIGLIA] Avvio SmartRower");

    calibStore.begin();
    loadCell.begin();

    pinMode(BTN_STANDALONE_PIN, INPUT_PULLUP);

    WiFi.mode(WIFI_AP_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_11dBm);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Errore inizializzazione ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(onEspNowRecv);

    bootTime = millis();
    Serial.println("[MANIGLIA] In attesa di Heartbeat per 3 secondi...");
    
    esp_wifi_set_channel(RP_CHANNEL, WIFI_SECOND_CHAN_NONE); // Canale 6 fisso (brief §5.4)

    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = RP_CHANNEL;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Errore aggiunta peer broadcast");
        return;
    }

    currentPacket.magic = 0xA1;
    currentPacket.seq = 0;
    currentPacket.flags = 0;

}

void loop() {
    unsigned long now = millis();

    // Selezione modalità
    if (!modeSelected) {
        // Pulsante premuto al boot → forza standalone (brief Fase 4)
        bool forceStandalone = (digitalRead(BTN_STANDALONE_PIN) == LOW);
        if (heartbeatReceived && !forceStandalone) {
            standaloneMode = false;
            modeSelected = true;
            Serial.println("[MANIGLIA] Heartbeat ricevuto! ModalitÃ : TRASMETTITORE");
            WiFi.mode(WIFI_STA); // Disattiva AP
            esp_wifi_set_channel(RP_CHANNEL, WIFI_SECOND_CHAN_NONE); // Ri-forza il canale 6
        } else if (forceStandalone || now - bootTime > 3000) {
            standaloneMode = true;
            modeSelected = true;
            Serial.println("[MANIGLIA] Nessun heartbeat. Modalità: STANDALONE");
            WiFi.softAP("RP_Handle", "password", RP_CHANNEL, 0, 4);
            physicsLite.begin();
            webUIStandalone.begin();
        } else {
            // Continua a chiamare loadCell per stabilizzarlo
            bool isNew = false;
            loadCell.getKg(isNew);
            delay(10);
            return;
        }
    }

    // Modalità Operativa
    bool isNew = false;
    float kg = loadCell.getKg(isNew);
    
    if (standaloneMode) {
        physicsLite.processForce(kg);
        webUIStandalone.handleClients();
        
        float* buf = physicsLite.getReadyBuffer();
        if (buf != nullptr) {
            webUIStandalone.sendTelemetryBuffer(buf);
            physicsLite.clearReadyBuffer();
        }
        
        static unsigned long lastMetrics = 0;
        if (millis() - lastMetrics > 500) {
            lastMetrics = millis();
            webUIStandalone.sendMetrics();
        }
        delay(2);
    } else {
        float centi = kg * 100.0f;
        if (centi > 32767.0f) centi = 32767.0f;
        if (centi < -32768.0f) centi = -32768.0f;
        int16_t centiKg = (int16_t)centi;
        if (abs(centiKg) > abs(currentPeakCenti)) {
            currentPeakCenti = centiKg;
        }

        static unsigned long lastTx = 0;
        if (now - lastTx >= 10) {
            lastTx = now;
            
            currentPacket.rawAdc = loadCell.getLastRaw();
            currentPacket.forceCenti = centiKg;
            currentPacket.peakCenti = currentPeakCenti;
            
            esp_err_t res = esp_now_send(broadcastAddress, (uint8_t *) &currentPacket, sizeof(ForcePacket));
            if (res != ESP_OK) Serial.printf("[ESP-NOW] send err: %d\n", res);
            
            currentPacket.seq++;
            currentPeakCenti = 0;
        }
    }
}
