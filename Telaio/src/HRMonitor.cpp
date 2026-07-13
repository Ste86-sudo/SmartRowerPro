#include "HRMonitor.h"
#include <NimBLEDevice.h>
#include "SharedData.h"

HRMonitor hrMonitor;

static BLEUUID hrServiceUUID("180D");
static BLEUUID hrCharUUID("2A37");
static bool doConnect = false;
static bool bleConnected = false;
static NimBLEAddress* pServerAddress = nullptr;
static NimBLEClient* pClient = nullptr;

static void setHeartRate(uint16_t hr) {
    portENTER_CRITICAL(&telemetryMux);
    telemetry.heartRate = hr;
    portEXIT_CRITICAL(&telemetryMux);
}

class HRClientCallback : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pclient) override {
        Serial.println("[BLE] Connesso alla Fascia Cardio!");
    }
    void onDisconnect(NimBLEClient* pclient) override {
        bleConnected = false;
        setHeartRate(0);
        Serial.println("[BLE] Disconnesso dalla Fascia Cardio!");
        NimBLEDevice::deleteClient(pclient);
        pClient = nullptr;
    }
};

static void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 2) return;
    uint8_t flags = pData[0];
    if ((flags & 0x01) == 0) {              // HR a 8 bit
        setHeartRate(pData[1]);
    } else if (length > 2) {                // HR a 16 bit little-endian
        setHeartRate((uint16_t)pData[1] | ((uint16_t)pData[2] << 8));
    }
}

static bool connectToServer() {
    Serial.println("[BLE] Tentativo di connessione...");
    if (NimBLEDevice::getClientListSize() > 0) {
        pClient = NimBLEDevice::getClientByPeerAddress(*pServerAddress);
        if (!pClient) pClient = NimBLEDevice::getDisconnectedClient();
    }
    if (!NimBLEDevice::getClientListSize() || !pClient) {
        pClient = NimBLEDevice::createClient();
        pClient->setClientCallbacks(new HRClientCallback(), true);
    }

    if (!pClient->connect(*pServerAddress)) {
        Serial.println("[BLE] Connessione fallita");
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        return false;
    }

    NimBLERemoteService* pService = pClient->getService(hrServiceUUID);
    if (pService == nullptr) {
        pClient->disconnect();
        return false;
    }

    NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(hrCharUUID);
    if (pChar == nullptr) {
        pClient->disconnect();
        return false;
    }

    if (pChar->canNotify()) {
        pChar->subscribe(true, notifyCallback);
    }
    bleConnected = true;
    return true;
}

class HRScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        if (dev->haveServiceUUID() && dev->isAdvertisingService(hrServiceUUID)) {
            if (!doConnect && !bleConnected) {
                Serial.print("[BLE] Trovata Fascia Cardio: ");
                Serial.println(dev->getAddress().toString().c_str());
                if (pServerAddress) delete pServerAddress;
                pServerAddress = new NimBLEAddress(dev->getAddress());
                NimBLEDevice::getScan()->stop();
                doConnect = true;
            }
        }
    }
};

static void scanEndedCB(NimBLEScanResults results) {
    NimBLEDevice::getScan()->clearResults();
}

void HRMonitor::begin() {
    NimBLEDevice::init("");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new HRScanCallbacks());
    // Scansione passiva a basso duty cycle (5%): minimizza l'impatto
    // sulla coesistenza con WiFi/ESP-NOW
    pScan->setActiveScan(false);
    pScan->setInterval(3200);
    pScan->setWindow(160);
    Serial.println("[BLE] NimBLE inizializzato.");
}

void HRMonitor::loop() {
    static unsigned long lastBleMs = 0;
    unsigned long now = millis();
    if (now - lastBleMs < 1000) return;
    lastBleMs = now;

    if (doConnect) {
        if (connectToServer()) {
            Serial.println("[BLE] Sottoscrizione HR completata.");
        } else {
            Serial.println("[BLE] Connessione fallita. Ripresa scansione lenta.");
            NimBLEDevice::getScan()->start(0, scanEndedCB, false);
        }
        doConnect = false;
    }
    if (!bleConnected && !doConnect && !NimBLEDevice::getScan()->isScanning()) {
        NimBLEDevice::getScan()->start(0, scanEndedCB, false);
    }
}
