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
#include "soc/gpio_reg.h"   // REG_READ / GPIO_IN_REG per lettura veloce dell'encoder in ISR

#define DEBUG_LOGS 0 // Metti a 1 per log seriali a 5Hz

// PINOUT ESP32-C3
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3
#define I2C_SDA 8
#define I2C_SCL 9

#define ENABLE_TOF 1 // Laser VL53L0X attivo (I2C su SDA=8, SCL=9, con pull-up)

Preferences prefs;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
bool tofConnected = false;

// Configurazione Rete e Peer
String wifiSSID = "";
String wifiPass = "";
String hrMacStr = "D8:EA:D5:6A:14:CD";
uint8_t peerMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

// Variabili sensori
volatile int32_t encoderTicks = 0;
unsigned long lastSendTime = 0;
unsigned long lastWsTime = 0;
const int sendIntervalMs = 20; // 50Hz ESP-NOW
const int wsIntervalMs = 200;  // 5Hz WebSocket (evita disconnessioni 1006 per buffer pieno)

// Struttura Payload
typedef struct {
    int32_t t;       // Ticks encoder
    uint16_t d;      // Distanza in mm
    uint8_t hr;      // Heart Rate
} __attribute__((packed)) PayloadData;

PayloadData payload;

// Tabella stati per quadratura 4X (anti-jitter puro)
static const int8_t enc_states[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};
volatile uint8_t old_AB = 0;
volatile int32_t raw_encoder_ticks_4x = 0;

void IRAM_ATTR readEncoder() {
    // Lettura simultanea di A e B con un solo accesso al registro GPIO: più veloce di
    // due digitalRead() e senza skew tra le fasi → riduce i passi persi ad alta velocità.
    uint32_t gpio = REG_READ(GPIO_IN_REG);
    uint8_t a = (gpio >> ENCODER_PIN_A) & 0x1;
    uint8_t b = (gpio >> ENCODER_PIN_B) & 0x1;
    old_AB <<= 2;
    old_AB |= (a << 1) | b;
    raw_encoder_ticks_4x += enc_states[(old_AB & 0x0F)];
    encoderTicks = raw_encoder_ticks_4x / 4;
}

void parseMacString(String macStr, uint8_t* macArr) {
    macStr.trim();
    macStr.replace("-", ":");
    int idx = 0;
    for (int i = 0; i < 6; i++) {
        int nextIdx = macStr.indexOf(':', idx);
        if (nextIdx == -1) nextIdx = macStr.length();
        macArr[i] = strtol(macStr.substring(idx, nextIdx).c_str(), NULL, 16);
        idx = nextIdx + 1;
    }
}

String formatMac(const uint8_t* mac) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

// --- BLE Globals (moved up for WS scope) ---
static BLEUUID hrServiceUUID("180D");
static BLEUUID hrCharUUID("2A37");
static boolean doConnect = false;
static boolean bleConnected = false;
static NimBLEAddress* pServerAddress = nullptr;
volatile uint8_t currentHR = 0;

void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
    if (t == WS_EVT_CONNECT) {
        Serial.printf("[WS] Client %u CONNECTED\n", c->id());
        // NON INVIARE DATI QUI! Causa crash 1006 su ESPAsyncWebServer
    } else if (t == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client %u DISCONNECTED\n", c->id());
    } else if (t == WS_EVT_ERROR) {
        Serial.printf("[WS] Client %u ERROR\n", c->id());
    } else if (t == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)a;
        if (info->final && info->index == 0 && info->len == l && info->opcode == WS_TEXT) {
            d[l] = 0; String msg = String((char*)d);
            Serial.printf("[WS] Ricevuto da %u: %s\n", c->id(), msg.c_str());
            if (msg == "GET_CFG") {
                Serial.println("[WS] Richiesta GET_CFG ricevuta");
                String cfgMsg = "CFG:" + formatMac(peerMac) + "," + wifiSSID + "," + wifiPass;
                c->text(cfgMsg);
            } else if (msg == "SCAN") {
                Serial.println("[WS] Richiesta SCAN ricevuta");
                WiFi.scanNetworks(true, true);
            } else if (msg == "REBOOT") {
                Serial.println("[WS] Richiesta REBOOT ricevuta");
                delay(500); ESP.restart();
            } else if (msg.startsWith("CFG:")) {
                // CFG:mac,ssid,pass,hrmac
                int c1 = msg.indexOf(',');
                int c2 = msg.indexOf(',', c1+1);
                int c3 = msg.indexOf(',', c2+1);
                if (c1 > 0 && c2 > 0) {
                    String macStr = msg.substring(4, c1);
                    wifiSSID = msg.substring(c1+1, c2);
                    if (c3 > 0) {
                        wifiPass = msg.substring(c2+1, c3);
                        hrMacStr = msg.substring(c3+1);
                        hrMacStr.trim();
                        prefs.putString("hrmac", hrMacStr);
                    } else {
                        wifiPass = msg.substring(c2+1);
                    }
                    
                    parseMacString(macStr, peerMac);
                    prefs.putString("mac", macStr);
                    prefs.putString("ssid", wifiSSID);
                    prefs.putString("pass", wifiPass);
                    Serial.println("[WS] Configurazione aggiornata!");
                }
            } else if (msg.startsWith("HRMAC:")) {
                hrMacStr = msg.substring(6);
                hrMacStr.trim();
                prefs.putString("hrmac", hrMacStr);
                Serial.println("[WS] Nuovo MAC Fascia Cardio salvato: " + hrMacStr);
                if (pServerAddress) { delete pServerAddress; pServerAddress = nullptr; }
                bleConnected = false; // Forza riconnessione
            }
        }
    }
}


// --- BLE Heart Rate Client ---

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
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    NimBLERemoteService* pRemoteService = pClient->getService(hrServiceUUID);
    if (pRemoteService == nullptr) {
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    NimBLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(hrCharUUID);
    if (pRemoteCharacteristic == nullptr) {
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->subscribe(true, notifyCallback);
    }
    bleConnected = true;
    return true;
}

class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(hrServiceUUID)) {
            // Filtro MAC parametrizzato
            if (hrMacStr.length() >= 17) {
                if (!advertisedDevice->getAddress().equals(NimBLEAddress(hrMacStr.c_str()))) {
                    return; // Ignora altre fasce
                }
            }
            if(!doConnect && !bleConnected) {
                Serial.print(F("[BLE] Trovata Fascia Cardio: "));
                Serial.println(advertisedDevice->getAddress().toString().c_str());
                if (pServerAddress) delete pServerAddress;
                // Prende l'indirizzo esatto con il tipo corretto (PUBLIC o RANDOM)
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
    Wire.setTimeOut(50);     // timeout I2C 50ms: evita blocchi infiniti del bus (loop single-core C3)
    // Due tentativi di init: l'avvio I2C del VL53L0X a volte fallisce al primo colpo
    if (!lox.begin() && !lox.begin()) {
        Serial.println(F("[TOF] Init VL53L0X fallita — sensore disabilitato"));
        tofConnected = false;
    } else {
        Wire.setClock(100000);        // 100 kHz: più robusto a rumore/cablaggi lunghi della slitta
        lox.startRangeContinuous(20); // ranging continuo ~50Hz (allineato al loop ESP-NOW)
        tofConnected = true;
        Serial.println(F("[TOF] VL53L0X pronto"));
    }
#else
    tofConnected = false;
    Serial.println(F("[TOF] VL53L0X bypassato (ENABLE_TOF = 0)"));
#endif

    // 3. Caricamento Configurazioni
    prefs.begin("c3config", false);
    // Usa MAC Broadcast di default per ignorare configurazioni errate precedenti
    parseMacString("FF:FF:FF:FF:FF:FF", peerMac);
    // Usa "RP_AP" e "password" come default per collegarsi all'S3
    wifiSSID = prefs.getString("ssid", "RP_AP");
    wifiPass = prefs.getString("pass", "password");
    hrMacStr = prefs.getString("hrmac", "d8:ea:d5:6a:14:cd");

    // 4. Inizializzazione NimBLE
    NimBLEDevice::init("");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pScan->setActiveScan(false); // Passive scan per disturbare meno possibile il Wi-Fi
    pScan->setInterval(3200); // 2000ms
    pScan->setWindow(160);    // 100ms (5% duty cycle invece di 50%)
    Serial.println(F("[BLE] NimBLE inizializzato."));

    // 5. Configurazione WiFi (STA con fallback su AP)
    WiFi.mode(WIFI_STA);
    bool staConnected = false;
    
    // Forza IP Statico in STA per esporre la WebUI a 192.168.4.10 ed evitare conflitti
    Serial.println(F("[WIFI] Configurazione IP Statico (192.168.4.10)"));
    IPAddress local_IP(192, 168, 4, 10);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(local_IP, gateway, subnet);
    
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
            
            // MAC auto-rilevato rimosso per usare il MAC Broadcast (FF:FF:FF:FF:FF:FF) in ESP-NOW
            
            staConnected = true;
        } else {
            Serial.println(F("\n[WIFI] Timeout! Fallback in SoftAP..."));
            WiFi.disconnect();
        }
    }
    
    if (!staConnected) {
        WiFi.mode(WIFI_AP_STA);
        Serial.println(F("[WIFI] Avvio SoftAP 'Rower_C3_Setup' (192.168.4.10)"));
        IPAddress apIP(192, 168, 4, 10);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP("Rower_C3_Setup", "rower123", 1, 0, 4);
        Serial.printf("[WIFI] Canale (SoftAP): %d\n", 1);
    }
    
    // ESP32-C3 requires WIFI_PS_MIN_MODEM when both WiFi and Bluetooth are active
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
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
    
    // 7. Attiva Interrupts Encoder alla fine (Quadratura 4X)
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), readEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), readEncoder, CHANGE);
    Serial.println(F("Setup completato! Inizio loop."));
}

void scanEndedCB(NimBLEScanResults results) {
    NimBLEDevice::getScan()->clearResults();
}

void loop() {
    unsigned long now = millis();

    // --- Invio ESP-NOW al S3 (50Hz) ---
    if (now - lastSendTime >= sendIntervalMs) {
        lastSendTime = now;
        payload.t  = encoderTicks;
        payload.hr = currentHR;
#if ENABLE_TOF
        // Aggiorna solo se c'è un nuovo campione valido; altrimenti mantiene l'ultimo valore
        // (niente azzeramenti che farebbero "saltare" la posizione della slitta a 0).
        if (tofConnected && lox.isRangeComplete()) {
            uint16_t r = lox.readRange();
            if (r > 0 && r < 2000) {   // scarta fuori-portata (~8190) e letture nulle
                // EMA leggero (3:1): riduce il jitter mantenendo reattività
                payload.d = (payload.d == 0) ? r : (uint16_t)((payload.d * 3 + r) >> 2);
            }
        }
#else
        payload.d = 0;
#endif
        esp_now_send(peerMac, (uint8_t*)&payload, sizeof(payload));
    }

    // --- Gestione BLE fascia cardio (1Hz max per non bloccare il loop) ---
    static unsigned long lastBleMs = 0;
    if (now - lastBleMs >= 1000) {
        lastBleMs = now;
        if (doConnect) {
            if (connectToServer()) {
                Serial.println(F("[BLE] Sottoscrizione HR completata."));
            } else {
                Serial.println(F("[BLE] Connessione fallita. Ripresa scansione lenta."));
                NimBLEDevice::getScan()->start(0, scanEndedCB, false);
            }
            doConnect = false;
        }
        if (!bleConnected && !doConnect && !NimBLEDevice::getScan()->isScanning()) {
            NimBLEDevice::getScan()->start(0, scanEndedCB, false);
        }
    }

    // --- Invio WebSocket ai client del C3 (5Hz) ---
    if (now - lastWsTime >= wsIntervalMs) {
        lastWsTime = now;
        if (ws.count() > 0) {
            char wsBuf[32];
            snprintf(wsBuf, sizeof(wsBuf), "SYNC:%d,%u,%u", payload.t, payload.d, payload.hr);
            ws.textAll(wsBuf);
#if DEBUG_LOGS
            // Debug invio:
            Serial.printf("[WS] Invio SYNC -> Ticks: %d, ToF: %u, HR: %u\n", payload.t, payload.d, payload.hr);
#endif
        }
    }

    // --- Pulizia client WebSocket disconnessi (ogni 2s) ---
    static unsigned long lastCleanup = 0;
    if (now - lastCleanup >= 2000) {
        lastCleanup = now;
        ws.cleanupClients(2);
    }

    // --- Risultati scansione WiFi asincrona (throttled 500ms) ---
    static unsigned long lastScanCheck = 0;
    if (now - lastScanCheck >= 500) {
        lastScanCheck = now;
        int16_t n = WiFi.scanComplete();
        if (n >= 0) {
            String res;
            res.reserve(256);
            res = "WIFI_LIST:";
            for (int i = 0; i < n; ++i) {
                if (i > 0) res += ",";
                res += WiFi.SSID(i);
            }
            ws.textAll(res);
            WiFi.scanDelete();
        }
    }
}
