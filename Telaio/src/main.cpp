#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoOTA.h>

#include "Config.h"
#include "RowerProtocol.h"
#include "SharedData.h"
#include "ConfigManager.h"
#include "Telemetry.h"
#include "Physics.h"
#include "WebUI.h"
#include "Encoder.h"
#include "HRMonitor.h"
#include "LaserSensor.h"

// Task di rete su core 0: OTA, BLE HR, heartbeat ESP-NOW, invio dati WebSocket
static void netTask(void* pvParameters) {
    unsigned long lastClean = 0;
    unsigned long lastMetrics = 0;

    for (;;) {
        ArduinoOTA.handle();
        hrMonitor.loop();
        TelemetryRadio::sendHeartbeat();

        unsigned long now = millis();

        if (now - lastClean >= 500) {
            lastClean = now;
            webApp.handleClients();
        }

        float* currentBuffer = Physics::getReadyBuffer();
        if (currentBuffer != nullptr) {
            webApp.sendTelemetryBuffer(currentBuffer);
            Physics::clearReadyBuffer();   // ricicla il buffer anche senza client connessi
        }

        if (now - lastMetrics >= 500) {
            lastMetrics = now;
            webApp.sendMetrics();
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static void setupWiFi() {
    WiFi.mode(WIFI_AP_STA);

    bool staConnected = false;

    // Tenta la rete di casa salvata in NVS, se presente
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

    // NOTA: con STA attiva il SoftAP segue il canale del router; ESP-NOW con la
    // maniglia (fissa su RP_CHANNEL) funziona solo se il router è sul canale 6.
    // Limite noto e accettato.
    uint8_t apChannel = staConnected ? WiFi.channel() : RP_CHANNEL;
    WiFi.softAP(WIFI_SSID, WIFI_PASS, apChannel, 0, 4);
    Serial.printf("[WIFI] SoftAP attivo sul canale: %d\n", apChannel);

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    WiFi.setTxPower(WIFI_POWER_11dBm);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== SMART ROWER 2.0 — TELAIO BOOT ===");

    // 1. Impostazioni da NVS (include wifiSSID/wifiPass)
    config.begin();

    // 2. Encoder (PCNT hardware)
    encoderManager.begin();

    // 3. WiFi (STA + SoftAP)
    setupWiFi();

    // 4. BLE fascia cardio
    hrMonitor.begin();

    Serial.println("==========================================");
    Serial.print(">>> MAC STA  (ESP-NOW da Maniglia): ");
    Serial.println(WiFi.macAddress());
    Serial.print(">>> MAC SoftAP: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.println("==========================================");

    // 5. Moduli di rete (ESP-NOW deve partire dopo il WiFi)
    TelemetryRadio::begin();
    webApp.begin();

    ArduinoOTA.setHostname("rowing-tracker");
    ArduinoOTA.setPassword("rower-ota");   // evita flash non autorizzati sull'AP
    ArduinoOTA.begin();

    xTaskCreatePinnedToCore(netTask, "NetTask", 8192, NULL, 1, NULL, 0);

    // 6. Motore fisico sul core 1
    Physics::begin();

    // 7. Sensore laser carrello (I2C)
    laserSensor.begin();

    Serial.println("[BOOT] Setup completato.");
}

void loop() {
    vTaskDelete(NULL);
}
