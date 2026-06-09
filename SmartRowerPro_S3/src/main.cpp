#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoOTA.h>

#include "Config.h"
#include "SharedData.h"
#include "ConfigManager.h"
#include "LoadCell.h"
#include "Telemetry.h"
#include "Physics.h"
#include "WebUI.h"



void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== SMART ROWER PRO BOOT ===");

    // 1. Carica le impostazioni salvate (include wifiSSID/wifiPass da NVS)
    config.begin();

    // 2. Inizializza l'hardware SPI e la cella di carico
    loadCell.begin();

    // 3. Configura il Wi-Fi
    WiFi.mode(WIFI_AP_STA);

    bool staConnected = false;

    // Tenta la rete salvata in NVS, se presente
    if (config.wifiSSID.length() > 0) {
        Serial.print("[WIFI] Connessione a: ");
        Serial.println(config.wifiSSID);
        WiFi.begin(config.wifiSSID.c_str(), config.wifiPass.c_str());
        unsigned long startMs = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 10000)) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[WIFI] Connesso! IP: " + WiFi.localIP().toString());
            Serial.printf("[WIFI] Canale: %d\n", WiFi.channel());
            staConnected = true;
        } else {
            Serial.println("\n[WIFI] Timeout STA. Avvio SoftAP...");
            WiFi.disconnect();
        }
    }

    // Alza sempre il SoftAP (rimane attivo anche se STA è connessa, stesso canale)
    WiFi.softAP(WIFI_SSID, WIFI_PASS, staConnected ? WiFi.channel() : 1, 0, 4);
    Serial.printf("[WIFI] SoftAP attivo sul canale: %d\n", staConnected ? WiFi.channel() : 1);

    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_11dBm);

    Serial.println("==========================================");
    Serial.print(">>> MAC STA  (ESP-NOW da C3): ");
    Serial.println(WiFi.macAddress());
    Serial.print(">>> MAC SoftAP: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.println("==========================================");

    // 4. Avvia i moduli di rete (ESP-NOW deve partire dopo WiFi)
    TelemetryRadio::begin();
    webApp.begin();

    ArduinoOTA.setHostname("rowing-tracker");
    ArduinoOTA.begin();



    // 5. Avvia il motore fisico sul CORE 1
    Physics::begin();

    Serial.println("[BOOT] Setup completato.");
}

void loop() {
    ArduinoOTA.handle();

    // Pulizia client WebSocket disconnessi (~500ms)
    static unsigned long lastClean = 0;
    if (millis() - lastClean >= 500) {
        lastClean = millis();
        webApp.handleClients();
    }

    // Invia i dati crudi ad alta frequenza al grafico (ping-pong buffer)
    float* currentBuffer = Physics::getReadyBuffer();
    if (currentBuffer != nullptr) {
        webApp.sendTelemetryBuffer(currentBuffer);
        Physics::clearReadyBuffer();
    }

    // Invia le metriche aggregate ogni 500ms
    static unsigned long lastMetrics = 0;
    if (millis() - lastMetrics >= 500) {
        lastMetrics = millis();
        webApp.sendMetrics();
    }



    vTaskDelay(pdMS_TO_TICKS(2));
}
