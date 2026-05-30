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

#if ENABLE_BLE
  #include "BLE_FTMS.h"
#endif

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== SMART ROWER PRO BOOT ===");

    // 1. Carica le impostazioni salvate
    config.begin();
    
    // 2. Inizializza l'hardware SPI
    loadCell.begin();

    // 3. Inizializza il Wi-Fi e l'ESP-NOW
    WiFi.mode(WIFI_AP_STA);
    
    bool staConnected = false;
    if (config.wifiSSID.length() > 0) {
        Serial.print("[WIFI] Tentativo di connessione a: ");
        Serial.println(config.wifiSSID);
        
        WiFi.disconnect();
        delay(100);
        WiFi.begin(config.wifiSSID.c_str(), config.wifiPass.c_str());
        unsigned long startMs = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 10000)) {
            delay(500);
            Serial.print(".");
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[WIFI] Connesso! IP: " + WiFi.localIP().toString());
            Serial.printf("[WIFI] Canale (router): %d\n", WiFi.channel());
            staConnected = true;
        } else {
            Serial.println("\n[WIFI] Timeout connessione STA! Fallback in SoftAP...");
            WiFi.disconnect();
        }
    }
    
    // Se non abbiamo un SSID o la connessione è fallita, alziamo il SoftAP
    if (!staConnected) {
        Serial.println("[WIFI] Avvio SoftAP...");
        WiFi.softAP(WIFI_SSID, WIFI_PASS, 1, 0, 4);
        Serial.printf("[WIFI] Canale (SoftAP): %d\n", 1);
    }

    esp_wifi_set_ps(WIFI_PS_NONE); // Disabilita Power Saving per stabilità
    WiFi.setTxPower(WIFI_POWER_11dBm);

    // Stampa MAC Address per debug ESP-NOW
    Serial.println("==========================================");
    Serial.print(">>> MAC STA (Per ESP-NOW dal C3): ");
    Serial.println(WiFi.macAddress());
    Serial.print(">>> MAC SoftAP: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.println("==========================================");

    // 4. Avvia i moduli di rete
    TelemetryRadio::begin();
    webApp.begin();
    
    ArduinoOTA.setHostname("rowing-tracker");
    ArduinoOTA.begin();

#if ENABLE_BLE
    bleManager.begin();
#endif

    // 5. Avvia il motore fisico sul CORE 1
    Physics::begin();

    Serial.println("[BOOT] Setup completato con successo.");
}

void loop() {
    ArduinoOTA.handle();
    
    static unsigned long lastClean = 0;
    if (millis() - lastClean >= 5000) { 
        lastClean = millis(); 
        webApp.handleClients();
    }

    // Invia i dati crudi ad alta frequenza al grafico e sblocca il buffer
    volatile float* currentBuffer = Physics::getReadyBuffer();
    if (currentBuffer != nullptr) {
        webApp.sendTelemetryBuffer(currentBuffer);
        Physics::clearReadyBuffer(); // Sblocca il buffer Ping-Pong per il Core 1
    }

    // Invia le metriche aggregate
    static unsigned long lastMetrics = 0;
    if (millis() - lastMetrics >= 500) { 
        lastMetrics = millis(); 
        webApp.sendMetrics();
    }

#if ENABLE_BLE
    static unsigned long lastBle = 0;
    if (millis() - lastBle >= 1000) { 
        lastBle = millis(); 
        bleManager.updateAndNotify();
    }
#endif

    // Respiro per lo scheduler TCP/IP
    vTaskDelay(pdMS_TO_TICKS(2));
}
