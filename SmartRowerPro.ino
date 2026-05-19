#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <SPI.h>
#include <ADS1220_WE.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "webapp.h"

// --- ESP32 Hardware Configuration ---
#define SPI_MISO          0
#define SPI_MOSI          1
#define SPI_SCK           2
#define ADS1220_CS_PIN    3
#define ADS_CLK_PIN       4
#define ADS1220_DRDY_PIN  5

// --- Wi-Fi SoftAP ---
const char* ssid = "RP_AP";
const char* password = "password";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
ADS1220_WE ads = ADS1220_WE(ADS1220_CS_PIN, ADS1220_DRDY_PIN);
Preferences prefs;

// --- Sensor and Persistent Profile Variables ---
float load = 0.0;
int32_t tareValue = 0;
float scaleFactor = 10000.0;
float userHeight = 181.0;
float userWeight = 75.0;
float pullThreshold = 3.0;
float releaseThreshold = 1.5;

// --- BLE Variables ---
BLEServer* pServer = NULL;
BLECharacteristic* pFtmsRowerData = NULL;
bool deviceConnected = false;

// FTMS profile: Fitness Machine Service (Rower for EXR / Kinomap)
#define FTMS_SERVICE_UUID         "1826"
#define ROWER_DATA_UUID           "2AD1"
#define FTMS_FEATURE_UUID         "2ACC"
#define FTMS_CONTROL_POINT_UUID   "2AD9"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; Serial.println("BLE app connected!"); }
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("BLE app disconnected. Restarting advertising...");
        BLEDevice::startAdvertising();
    }
};

class FTMSControlPointCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        uint8_t* data = pCharacteristic->getData();
        size_t len = pCharacteristic->getLength();
        if (len > 0) {
            uint8_t opCode = data[0];
            uint8_t response[3] = {0x80, opCode, 0x01};
            pCharacteristic->setValue(response, 3);
            pCharacteristic->notify();
        }
    }
};

// webapp.h contains the index_html[] PROGMEM string (HTML/CSS/JS)

// =================================================================================
// WEBSOCKET HANDLER
// =================================================================================
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        String cmd = String((char*)data);
        cmd.trim();

        if (cmd == "tare") {
            tareValue = ads.getRawData();
            prefs.putInt("tare", tareValue);
        }
        else if (cmd.startsWith("calib:")) {
            scaleFactor = (float)(ads.getRawData() - tareValue) / cmd.substring(6).toFloat();
            prefs.putFloat("scale", scaleFactor);
        }
        else if (cmd.startsWith("profile:")) {
            int sep1 = cmd.indexOf('|');
            int sep2 = cmd.indexOf('|', sep1 + 1);
            int sep3 = cmd.indexOf('|', sep2 + 1);

            if (sep1 > 0 && sep2 > 0 && sep3 > 0) {
                userHeight = cmd.substring(8, sep1).toFloat();
                userWeight = cmd.substring(sep1 + 1, sep2).toFloat();
                pullThreshold = cmd.substring(sep2 + 1, sep3).toFloat();
                releaseThreshold = cmd.substring(sep3 + 1).toFloat();

                prefs.putFloat("uHeight", userHeight);
                prefs.putFloat("uWeight", userWeight);
                prefs.putFloat("uPull", pullThreshold);
                prefs.putFloat("uRel", releaseThreshold);
            }
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) handleWebSocketMessage(arg, data, len);
    if (type == WS_EVT_CONNECT) {
        client->text("CONFIG:" + String(userHeight, 1) + "|" + String(userWeight, 1) + "|" + String(pullThreshold, 1) + "|" + String(releaseThreshold, 1));
    }
}

// =================================================================================
// ESP32 SETUP
// =================================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    prefs.begin("rower", false);
    tareValue = prefs.getInt("tare", 0);
    scaleFactor = prefs.getFloat("scale", 10000.0);
    userHeight = prefs.getFloat("uHeight", 181.0);
    userWeight = prefs.getFloat("uWeight", 75.0);
    pullThreshold = prefs.getFloat("uPull", 3.0);
    releaseThreshold = prefs.getFloat("uRel", 1.5);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    ads.init();
    ads.setCompareChannels(ADS1220_MUX_1_2);
    ads.setVRefSource(ADS1220_VREF_AVDD_AVSS);
    ads.setGain(ADS1220_GAIN_128);
    ads.setDataRate(ADS1220_DR_LVL_6);
    ads.setConversionMode(ADS1220_CONTINUOUS);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", (const uint8_t*)index_html, sizeof(index_html) - 1);
    });

    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<!DOCTYPE html><html><head><title>OTA Update</title><style>body{background:#020617; color:#fff; text-align:center; font-family:sans-serif; padding:50px;}</style></head><body><h2>Firmware Update</h2><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update' accept='.bin' style='padding:20px;'><br><input type='submit' value='START FLASH' style='padding:10px 20px; background:#22d3ee; color:#000; font-weight:bold; border:none; cursor:pointer; border-radius:5px;'></form><br><br><button onclick=\"window.location.href='/'\" style='padding:10px; background:#334155; color:white; border:none; border-radius:5px; cursor:pointer;'>Back to Dashboard</button></body></html>";
        request->send(200, "text/html", html);
    });
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        bool shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK: Rebooting..." : "FLASH ERROR");
        response->addHeader("Connection", "close");
        request->send(response);
        if(shouldReboot) { delay(500); ESP.restart(); }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if(!index) Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
        if(!Update.hasError()) Update.write(data, len);
        if(final) Update.end(true);
    });

    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.begin();
    ArduinoOTA.setHostname("rowing-tracker");
    ArduinoOTA.begin();

    // --- Bluetooth FTMS setup (EXR / Kinomap) ---
    BLEDevice::init("Smart Rower Pro");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pFtmsService = pServer->createService(FTMS_SERVICE_UUID);
    BLECharacteristic *pFtmsFeature = pFtmsService->createCharacteristic(FTMS_FEATURE_UUID, BLECharacteristic::PROPERTY_READ);
    uint8_t ftmsFeatureData[8] = {0x26, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    pFtmsFeature->setValue(ftmsFeatureData, 8);

    pFtmsRowerData = pFtmsService->createCharacteristic(ROWER_DATA_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pFtmsRowerData->addDescriptor(new BLE2902());
    BLECharacteristic *pFtmsControlPoint = pFtmsService->createCharacteristic(FTMS_CONTROL_POINT_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    pFtmsControlPoint->addDescriptor(new BLE2902());
    pFtmsControlPoint->setCallbacks(new FTMSControlPointCallbacks());

    pFtmsService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(FTMS_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
}

// =================================================================================
// ESP32 MAIN LOOP — BLE broadcast at 1 Hz
// =================================================================================
void loop() {
    ArduinoOTA.handle();
    ws.cleanupClients();

    static unsigned long lastBleNotify = 0;

    // 1 kHz sensor acquisition state
    static unsigned long lastMicros = 0;
    static float batchBuffer[100]; // 100 samples per batch → dispatched at 10 Hz
    static int batchIndex = 0;

    static bool espIsPulling = false;
    static float espCurrentPeak = 0;
    static unsigned long espLastStrokeTime = 0;
    static uint16_t cumulativeStrokes = 0;
    static float espTotalDistance = 0.0;
    static uint16_t currentWatts = 0;
    static uint16_t spm = 0;
    static uint16_t paceSeconds = 0;

    // --- 1 kHz sensor read ---
    unsigned long currentMicros = micros();
    if (currentMicros - lastMicros >= 1000) {
        lastMicros = currentMicros;
        load = ((float)ads.getRawData() - (float)tareValue) / scaleFactor;

        if (load > pullThreshold) {
            espIsPulling = true;
            if (load > espCurrentPeak) espCurrentPeak = load;
        }
        else if (load < releaseThreshold && espIsPulling) {
            espIsPulling = false;
            unsigned long now = millis();

            if (espLastStrokeTime > 0) {
                float durationSecs = (now - espLastStrokeTime) / 1000.0;
                spm = (uint16_t)(60.0 / durationSecs);
                float strokeLengthMeters = userHeight * 0.007;
                float workJoules = (espCurrentPeak * 0.6 * 9.81) * strokeLengthMeters;
                currentWatts = (uint16_t)(workJoules / durationSecs);

                float speedMs = 0;
                if (currentWatts > 0) speedMs = pow((float)currentWatts / 2.8, 1.0/3.0);
                espTotalDistance += (speedMs * durationSecs);
                if (speedMs > 0) paceSeconds = (uint16_t)(500.0 / speedMs);

                cumulativeStrokes++;
            }
            espLastStrokeTime = now;
            espCurrentPeak = 0;
        }

        batchBuffer[batchIndex++] = forceKg;

        // Send every 100 ms (10 Hz) to reduce Wi-Fi load
        if (batchIndex >= 100) {
            if(ws.count() > 0) {
                String payload;
                payload.reserve(600);
                payload = "B:";
                for(int i = 0; i < 100; i++) {
                    payload += String(batchBuffer[i], 1);
                    if (i < 99) payload += ",";
                }
                ws.textAll(payload);
            }
            batchIndex = 0;
        }
    }

    // --- Continuous BLE broadcast (1 Hz) ---
    if (deviceConnected && (millis() - lastBleNotify >= 1000)) {
        lastBleNotify = millis();
        // Reset metrics if no stroke detected for 4 seconds
        if (millis() - espLastStrokeTime > 4000) {
            currentWatts = 0;
            spm = 0;
            paceSeconds = 0;
        }

        uint16_t ftmsFlags = 0x002C;
        uint8_t strokeRate = (spm * 2) > 255 ? 255 : (uint8_t)(spm * 2);
        uint32_t dist24 = (uint32_t)espTotalDistance;

        uint8_t ftmsPayload[12];
        ftmsPayload[0] = ftmsFlags & 0xFF;
        ftmsPayload[1] = (ftmsFlags >> 8) & 0xFF;
        ftmsPayload[2] = strokeRate;
        ftmsPayload[3] = cumulativeStrokes & 0xFF;
        ftmsPayload[4] = (cumulativeStrokes >> 8) & 0xFF;
        ftmsPayload[5] = dist24 & 0xFF;
        ftmsPayload[6] = (dist24 >> 8) & 0xFF;
        ftmsPayload[7] = (dist24 >> 16) & 0xFF;
        ftmsPayload[8] = paceSeconds & 0xFF;
        ftmsPayload[9] = (paceSeconds >> 8) & 0xFF;
        ftmsPayload[10] = currentWatts & 0xFF;
        ftmsPayload[11] = (currentWatts >> 8) & 0xFF;

        pFtmsRowerData->setValue(ftmsPayload, 12);
        pFtmsRowerData->notify();
    }
}
