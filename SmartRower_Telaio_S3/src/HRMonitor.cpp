#include "HRMonitor.h"
#include <NimBLEDevice.h>
#include "SharedData.h"

HRMonitor hrMonitor;

static BLEUUID hrServiceUUID("180D");
static BLEUUID hrCharUUID("2A37");
static boolean doConnect = false;
static boolean bleConnected = false;
static NimBLEAddress* pServerAddress = nullptr;
static NimBLEClient* pClient = nullptr;

class MyClientCallback : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pclient) override {
        Serial.println("[BLE] Connesso alla Fascia Cardio!");
    }
    void onDisconnect(NimBLEClient* pclient) override {
        bleConnected = false;
        telemetry.heartRate = 0;
        Serial.println("[BLE] Disconnesso dalla Fascia Cardio!");
        NimBLEDevice::deleteClient(pclient);
        pClient = nullptr;
    }
};

void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length > 1) {
        uint8_t flags = pData[0];
        if ((flags & 0x01) == 0) { // 8-bit HR
            telemetry.heartRate = pData[1];
        } else if (length > 2) { // 16-bit HR: byte basso + byte alto
            uint16_t hr16 = (uint16_t)pData[1] | ((uint16_t)pData[2] << 8);
            telemetry.heartRate = hr16;
        }
    }
}


bool connectToServer() {
    Serial.println("[BLE] Tentativo di connessione...");
    if (NimBLEDevice::getClientListSize() > 0) {
        pClient = NimBLEDevice::getClientByPeerAddress(*pServerAddress);
        if (!pClient) pClient = NimBLEDevice::getDisconnectedClient();
    }
    if (!NimBLEDevice::getClientListSize() || !pClient) {
        pClient = NimBLEDevice::createClient();
        pClient->setClientCallbacks(new MyClientCallback(), true);
    }

    if (!pClient->connect(*pServerAddress)) {
        Serial.println("[BLE] Connessione fallita");
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
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

    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->subscribe(true, notifyCallback);
    }
    bleConnected = true;
    return true;
}

class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(hrServiceUUID)) {
            if(!doConnect && !bleConnected) {
                Serial.print("[BLE] Trovata Fascia Cardio: ");
                Serial.println(advertisedDevice->getAddress().toString().c_str());
                if (pServerAddress) delete pServerAddress;
                pServerAddress = new NimBLEAddress(advertisedDevice->getAddress());
                NimBLEDevice::getScan()->stop();
                doConnect = true;
            }
        }
    }
};

void scanEndedCB(NimBLEScanResults results) {
    NimBLEDevice::getScan()->clearResults();
}

void HRMonitor::begin() {
    NimBLEDevice::init("");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pScan->setActiveScan(false); 
    pScan->setInterval(3200); 
    pScan->setWindow(160);    
    Serial.println("[BLE] NimBLE inizializzato.");
}

void HRMonitor::loop() {
    unsigned long now = millis();
    static unsigned long lastBleMs = 0;
    if (now - lastBleMs >= 1000) {
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
}
