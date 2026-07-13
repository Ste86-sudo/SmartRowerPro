#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoOTA.h>
#include "Config.h"
#include "RowerProtocol.h"
#include "LoadCell.h"
#include "CalibStore.h"
#include "EspNowLink.h"
#include "PhysicsLite.h"
#include "WebUIStandalone.h"

// Modalità: TRASMETTITORE (heartbeat dal telaio entro 3 s) o STANDALONE
// (nessun heartbeat, oppure pulsante premuto al boot)
static bool standaloneMode = false;
static bool modeSelected = false;
static unsigned long bootTime = 0;

static int16_t currentPeakCenti = 0;

static void selectMode(unsigned long now) {
    bool forceStandalone = (digitalRead(BTN_STANDALONE_PIN) == LOW);

    if (espNowLink.heartbeatReceived() && !forceStandalone) {
        standaloneMode = false;
        modeSelected = true;
        Serial.println("[MANIGLIA] Heartbeat ricevuto! Modalità: TRASMETTITORE");
        WiFi.mode(WIFI_STA);        // disattiva l'AP
        espNowLink.lockChannel();   // ri-forza il canale dopo il cambio modo
    } else if (forceStandalone || now - bootTime > 3000) {
        standaloneMode = true;
        modeSelected = true;
        Serial.println("[MANIGLIA] Nessun heartbeat. Modalità: STANDALONE");
        // Stesso SSID del telaio: il telefono usa un'unica rete salvata
        WiFi.softAP("RP_AP", "password", RP_CHANNEL, 0, 4);
        physicsLite.begin();
        webUIStandalone.begin();

        // OTA anche via espota/PlatformIO (oltre alla pagina /update)
        ArduinoOTA.setHostname("rower-handle");
        ArduinoOTA.setPassword("rower-ota");
        ArduinoOTA.begin();
    }
}

// Comandi di calibrazione ricevuti dal telaio via ESP-NOW
static void processRemoteCommand() {
    uint8_t cmd;
    float param;
    if (!espNowLink.fetchCommand(cmd, param)) return;

    switch (cmd) {
        case CMD_TARE:
            calibStore.saveCalibration(loadCell.getLastRaw(), calibStore.getScale());
            Serial.println("[ESP-NOW] Ricevuto TARE da Telaio (eseguito)");
            break;

        case CMD_CALIB:
            if (param > 0.01f) {
                float delta = (float)loadCell.getLastRaw() - (float)calibStore.getTare();
                float newScale = delta / param;
                if (fabsf(newScale) > 0.001f) {
                    calibStore.saveCalibration(calibStore.getTare(), newScale);
                    Serial.println("[ESP-NOW] Ricevuto CALIB da Telaio (eseguito)");
                }
            }
            break;

        case CMD_SET_TARE: {
            uint32_t tRaw;
            memcpy(&tRaw, &param, 4);   // int32 bit-cast nel campo float
            calibStore.saveCalibration((int32_t)tRaw, calibStore.getScale());
            Serial.printf("[ESP-NOW] Ricevuto SET_TARE da Telaio: %d\n", (int32_t)tRaw);
            break;
        }

        case CMD_SET_SCALE:
            calibStore.saveCalibration(calibStore.getTare(), param);
            Serial.printf("[ESP-NOW] Ricevuto SET_SCALE da Telaio: %.4f\n", param);
            break;
    }
}

static void loopTransmitter(float kg, unsigned long now) {
    float centi = kg * 100.0f;
    if (centi > 32767.0f) centi = 32767.0f;
    if (centi < -32768.0f) centi = -32768.0f;
    int16_t centiKg = (int16_t)centi;
    if (abs(centiKg) > abs(currentPeakCenti)) {
        currentPeakCenti = centiKg;
    }

    static unsigned long lastTx = 0;
    if (now - lastTx >= 10) {   // 100 Hz
        lastTx = now;
        espNowLink.sendForce(loadCell.getLastRaw(), centiKg, currentPeakCenti);
        currentPeakCenti = 0;
    }
}

static void loopStandalone(float kg) {
    ArduinoOTA.handle();
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
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[MANIGLIA] Avvio SmartRower 2.0");

    calibStore.begin();
    loadCell.begin();

    pinMode(BTN_STANDALONE_PIN, INPUT_PULLUP);

    WiFi.mode(WIFI_AP_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_11dBm);

    if (!espNowLink.begin()) return;

    bootTime = millis();
    Serial.println("[MANIGLIA] In attesa di Heartbeat per 3 secondi...");
}

void loop() {
    unsigned long now = millis();

    if (!modeSelected) {
        selectMode(now);
        if (!modeSelected) {
            // In attesa: continua a leggere la cella per stabilizzarla
            bool isNew = false;
            loadCell.getKg(isNew);
            delay(10);
            return;
        }
    }

    processRemoteCommand();

    bool isNew = false;
    float kg = loadCell.getKg(isNew);

    if (standaloneMode) {
        loopStandalone(kg);
    } else {
        loopTransmitter(kg, now);
    }
}
