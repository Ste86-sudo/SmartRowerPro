#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoOTA.h>

#include "Config.h"
#include "SharedData.h"
#include "ConfigManager.h"
#include "Telemetry.h"
#include "Physics.h"
#include "WebUI.h"
#include "Encoder.h"
#include "HRMonitor.h"
#include "LaserSensor.h"


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== SMART ROWER PRO BOOT ===");

    // 1. Carica le impostazioni salvate (include wifiSSID/wifiPass da NVS)
    config.begin();

    // 2. Inizializza Encoder
    encoderManager.begin();

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

    // NOTA STA: se la connessione STA è attiva, il SoftAP segue il canale del router.
    // ESP-NOW con la maniglia (bloccata su RP_CHANNEL) funzionerà solo se il router è sul canale 6.
    // È un limite noto accettato per l'uso futuro della modalità STA.
    uint8_t apChannel = staConnected ? WiFi.channel() : RP_CHANNEL;
    WiFi.softAP(WIFI_SSID, WIFI_PASS, apChannel, 0, 4);
    Serial.printf("[WIFI] SoftAP attivo sul canale: %d\n", apChannel);

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    WiFi.setTxPower(WIFI_POWER_11dBm);

    // Inizializza HR BLE
    hrMonitor.begin();

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
    ArduinoOTA.setPassword("rower-ota");   // evita flash non autorizzati sull'AP
    ArduinoOTA.begin();

    xTaskCreatePinnedToCore([](void* p){
        for(;;) {
            ArduinoOTA.handle();
            hrMonitor.loop();
            TelemetryRadio::sendHeartbeat();

            static unsigned long lastClean = 0;
            if (millis() - lastClean >= 500) {
                lastClean = millis();
                webApp.handleClients();
            }

            float* currentBuffer = Physics::getReadyBuffer();
            if (currentBuffer != nullptr) {
                webApp.sendTelemetryBuffer(currentBuffer);
                Physics::clearReadyBuffer();   // garantisce il riciclo del buffer anche senza client connessi
            }

            static unsigned long lastMetrics = 0;
            if (millis() - lastMetrics >= 500) {
                lastMetrics = millis();
                webApp.sendMetrics();
            }
            
            vTaskDelay(pdMS_TO_TICKS(2));
        }
    }, "NetTask", 8192, NULL, 1, NULL, 0);



    // 5. Avvia sensore Laser (I2C)
    laserSensor.begin();

    // 6. Avvia il motore fisico sul CORE 1
    Physics::begin();

    Serial.println("[BOOT] Setup completato.");
}

void loop() {
    vTaskDelete(NULL);
}
