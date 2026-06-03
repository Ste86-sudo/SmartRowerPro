#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include "WebUI_HTML.h"
#include <NimBLEDevice.h>

// PINOUT ESP32-C3
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define I2C_SDA 8
#define I2C_SCL 9

#define ENABLE_TOF 0 // Metti a 1 quando avrai collegato fisicamente il laser VL53L0X

Preferences prefs;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
bool tofConnected = false;

// Configurazione Rete e Peer
String wifiSSID = "";
String wifiPass = "";
uint8_t peerMac[6] = {0xAE, 0x27, 0x6E, 0xCB, 0xF4, 0x30};
esp_now_peer_info_t peerInfo;

// Variabili sensori
volatile int32_t encoderTicks = 0;
unsigned long lastSendTime = 0;
unsigned long lastWsTime = 0;
const int sendIntervalMs = 20; // 50Hz ESP-NOW
const int wsIntervalMs = 50;   // 20Hz WebSocket

// Struttura Payload
typedef struct {
    int32_t t;       // Ticks encoder
    uint16_t d;      // Distanza in mm
    uint8_t hr;      // Heart Rate
} __attribute__((packed)) PayloadData;

PayloadData payload;

// Interrupt Encoder
void IRAM_ATTR readEncoder() {
    int b = digitalRead(ENCODER_PIN_B);
    if(b > 0) {
        encoderTicks++;
    } else {
        encoderTicks--;
    }
}

void parseMacString(String macStr, uint8_t* macArr) {
    macStr.trim();
    if (macStr.length() == 17) {
        char* ptr;
        macArr[0] = strtol(macStr.substring(0, 2).c_str(), &ptr, 16);
        macArr[1] = strtol(macStr.substring(3, 5).c_str(), &ptr, 16);
        macArr[2] = strtol(macStr.substring(6, 8).c_str(), &ptr, 16);
        macArr[3] = strtol(macStr.substring(9, 11).c_str(), &ptr, 16);
        macArr[4] = strtol(macStr.substring(12, 14).c_str(), &ptr, 16);
        macArr[5] = strtol(macStr.substring(15, 17).c_str(), &ptr, 16);
    }
}

String formatMac(const uint8_t* mac) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
    if (t == WS_EVT_CONNECT) {
        String cfgMsg = "CFG:" + formatMac(peerMac) + "," + wifiSSID + "," + wifiPass;
        c->text(cfgMsg);
    } else if (t == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)a;
        if (info->final && info->index == 0 && info->len == l && info->opcode == WS_TEXT) {
            d[l] = 0; String msg = String((char*)d);
            if (msg == "SCAN") {
                WiFi.scanNetworks(true, true);
            } else if (msg == "REBOOT") {
                delay(500); ESP.restart();
            } else if (msg.startsWith("CFG:")) {
                // CFG:mac,ssid,pass
                int c1 = msg.indexOf(',');
                int c2 = msg.indexOf(',', c1+1);
                if (c1 > 0 && c2 > 0) {
                    String macStr = msg.substring(4, c1);
                    wifiSSID = msg.substring(c1+1, c2);
                    wifiPass = msg.substring(c2+1);
                    
                    parseMacString(macStr, peerMac);
                    prefs.putString("mac", macStr);
                    prefs.putString("ssid", wifiSSID);
                    prefs.putString("pass", wifiPass);
                }
            }
        }
    }
}


// --- BLE Heart Rate Client ---
static BLEUUID hrServiceUUID("180D");
static BLEUUID hrCharUUID("2A37");
static boolean doConnect = false;
static boolean bleConnected = false;
static NimBLEAddress* pServerAddress = nullptr;
volatile uint8_t currentHR = 0;

class MyClientCallback : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pclient) override {
        Serial.println(F("[BLE] Connesso alla Fascia Cardio!"));
    }
    void onDisconnect(NimBLEClient* pclient) override {
        bleConnected = false;
        currentHR = 0;
        Serial.println(F("[BLE] Disconnesso dalla Fascia Cardio!"));
    }
};

void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length > 1) {
        uint8_t flags = pData[0];
        if ((flags & 0x01) == 0) { // 8-bit HR
            currentHR = pData[1];
        } else if (length > 2) { // 16-bit HR
            currentHR = pData[1]; 
        }
    }
}

bool connectToServer() {
    Serial.println(F("[BLE] Tentativo di connessione..."));
    NimBLEClient* pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback(), false);
    
    if (!pClient->connect(*pServerAddress)) {
        Serial.println(F("[BLE] Connessione fallita"));
        return false;
    }
    
    NimBLERemoteService* pRemoteService = pClient->getService(hrServiceUUID);
    if (pRemoteService == nullptr) {
        pClient->disconnect();
        return false;
    }
    
    NimBLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(hrCharUUID);
    if (pRemoteCharacteristic == nullptr) {
        pClient->disconnect();
        return false;
    }
    
    if(pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->subscribe(true, notifyCallback);
    }
    bleConnected = true;
    return true;
}

class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(hrServiceUUID)) {
            if(!doConnect && !bleConnected) {
                Serial.print(F("[BLE] Trovata Fascia Cardio: "));
                Serial.println(advertisedDevice->getAddress().toString().c_str());
                if (pServerAddress) delete pServerAddress;
                pServerAddress = new NimBLEAddress(advertisedDevice->getAddress());
                NimBLEDevice::getScan()->stop();
                doConnect = true;
            }
        }
    }
};
// ------------------------------

void setup() {
    delay(2000);
    Serial.begin(115200);
    Serial.println(F("\n--- Avvio ESP32-C3 Rower Sensor ---"));
    
    // 1. Inizializzazione Pin Encoder
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);

    // 2. Inizializzazione I2C e Sensore ToF
#if ENABLE_TOF
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!lox.begin()) {
        Serial.println(F("[TOF] Failed to boot VL53L0X"));
        tofConnected = false;
    } else {
        Serial.println(F("[TOF] VL53L0X Ready"));
        lox.startRangeContinuous(20); 
        tofConnected = true;
    }
#else
    tofConnected = false;
    Serial.println(F("[TOF] VL53L0X bypassato (ENABLE_TOF = 0)"));
#endif

    // 3. Caricamento Configurazioni
    prefs.begin("c3config", false);
    String savedMac = prefs.getString("mac", "AE:27:6E:CB:F4:30");
    parseMacString(savedMac, peerMac);
    wifiSSID = prefs.getString("ssid", "");
    wifiPass = prefs.getString("pass", "");
    
    // 4. Configurazione WiFi (STA con fallback su AP)
    WiFi.mode(WIFI_AP_STA);
    bool staConnected = false;
    
    if (wifiSSID.length() > 0) {
        Serial.print(F("[WIFI] Tentativo connessione STA a: "));
        Serial.println(wifiSSID);
        WiFi.disconnect();
        delay(100);
        WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
        unsigned long startMs = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 10000)) {
            delay(500); Serial.print(".");
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print(F("\n[WIFI] Connesso! IP: "));
            Serial.println(WiFi.localIP().toString());
            Serial.printf("[WIFI] Canale (Router): %d\n", WiFi.channel());
            staConnected = true;
        } else {
            Serial.println(F("\n[WIFI] Timeout! Fallback in SoftAP..."));
            WiFi.disconnect();
        }
    }
    
    if (!staConnected) {
        Serial.println(F("[WIFI] Avvio SoftAP 'Rower_C3_Setup' (192.168.4.1)"));
        WiFi.softAP("Rower_C3_Setup", "rower123", 1, 0, 4);
        Serial.printf("[WIFI] Canale (SoftAP): %d\n", 1);
    }
    
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_11dBm);

    // 5. Inizializzazione Web Server e OTA
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ 
        AsyncWebServerResponse *response = r->beginResponse_P(200, "text/html", index_html_gz, sizeof(index_html_gz));
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        r->send(response);
    });

    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *r){
        bool ok = !Update.hasError();
        AsyncWebServerResponse *res = r->beginResponse(200, "text/plain", ok ? "OK" : "ERR");
        res->addHeader("Connection", "close"); r->send(res);
        if(ok) { delay(500); ESP.restart(); }
    }, [](AsyncWebServerRequest *r, String f, size_t i, uint8_t *d, size_t l, bool fin) {
        if(!i) Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
        if(!Update.hasError()) Update.write(d, l); if(fin) Update.end(true);
    });

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();

    // 6. Inizializzazione ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println(F("[ESPNOW] Errore inizializzazione!"));
    } else {
        Serial.println(F("[ESPNOW] Inizializzato."));
        memcpy(peerInfo.peer_addr, peerMac, 6);
        // Usa il canale attuale del Wi-Fi (fondamentale in modalità STA!)
        peerInfo.channel = staConnected ? WiFi.channel() : 1;  
        peerInfo.ifidx = staConnected ? WIFI_IF_STA : WIFI_IF_AP;
        peerInfo.encrypt = false;
        
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println(F("[ESPNOW] Failed to add peer S3"));
        } else {
            Serial.print(F("[ESPNOW] Peer registrato. Canale: "));
            Serial.println(peerInfo.channel);
        }
    }
    
    // 7. Attiva Interrupts Encoder alla fine
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), readEncoder, RISING);
    Serial.println(F("Setup completato! Inizio loop."));
}

void loop() {
    unsigned long now = millis();
    
    // Lettura e Invio ESP-NOW (50Hz)
    if (now - lastSendTime >= sendIntervalMs) {
        lastSendTime = now;
        
        
    // Gestione BLE Client
    if (doConnect) {
        if (connectToServer()) {
            Serial.println(F("[BLE] Sottoscrizione HR completata."));
        } else {
            Serial.println(F("[BLE] Connessione fallita. Ripresa scansione."));
            NimBLEDevice::getScan()->start(0, false);
        }
        doConnect = false;
    }
    if (!bleConnected && !doConnect && !NimBLEDevice::getScan()->isScanning()) {
         NimBLEDevice::getScan()->start(0, false);
    }
    
    payload.hr = currentHR;

        payload.t = encoderTicks;
        
#if ENABLE_TOF
        if (tofConnected && lox.isRangeComplete()) {
            payload.d = lox.readRange();
        } else {
            payload.d = 0;
        }
#else
        payload.d = 0;
#endif
        
        esp_now_send(peerMac, (uint8_t *) &payload, sizeof(payload));
    }
    
    // Invio WebSocket ai client (20Hz)
    if (now - lastWsTime >= wsIntervalMs) {
        lastWsTime = now;
        if (ws.count() > 0) {
            char wsBuf[32];
            snprintf(wsBuf, sizeof(wsBuf), "SYNC:%d,%u", payload.t, payload.d);
            ws.textAll(wsBuf);
        }
    }
    
    ws.cleanupClients();
    
    int16_t n = WiFi.scanComplete();
    if (n >= 0) {
        String res = "WIFI_LIST:";
        for (int i = 0; i < n; ++i) {
            if (i > 0) res += ",";
            res += WiFi.SSID(i);
        }
        ws.textAll(res);
        WiFi.scanDelete();
    }
}
