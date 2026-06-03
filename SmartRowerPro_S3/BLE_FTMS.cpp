#include "BLE_FTMS.h"
#if ENABLE_BLE
#include "SharedData.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define FTMS_SERVICE_UUID         "1826"
#define ROWER_DATA_UUID           "2AD1"
#define FTMS_FEATURE_UUID         "2ACC"
#define FTMS_CONTROL_POINT_UUID   "2AD9"

class BLEFTMSCallbacks : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
    bool connected = false;
    void onConnect(BLEServer* p) override { connected = true; }
    void onDisconnect(BLEServer* p) override { connected = false; BLEDevice::startAdvertising(); }
    void onWrite(BLECharacteristic *p) override {
        if (p->getLength() > 0) {
            uint8_t response[3] = {0x80, p->getData()[0], 0x01};
            p->setValue(response, 3); p->notify();
        }
    }
};

BLEFTMSCallbacks bleCallbacks;
BLEServer* pServer = nullptr;
BLECharacteristic* pData = nullptr;

BLE_FTMS bleManager;

void BLE_FTMS::begin() {
    BLEDevice::init("Smart Rower Pro");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(&bleCallbacks);
    
    BLEService *pFtms = pServer->createService(FTMS_SERVICE_UUID);
    BLECharacteristic *pFeat = pFtms->createCharacteristic(FTMS_FEATURE_UUID, BLECharacteristic::PROPERTY_READ);
    uint8_t featData[8] = {0x26, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
    pFeat->setValue(featData, 8);
    
    pData = pFtms->createCharacteristic(ROWER_DATA_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pData->addDescriptor(new BLE2902());
    
    BLECharacteristic *pCtrl = pFtms->createCharacteristic(FTMS_CONTROL_POINT_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    pCtrl->addDescriptor(new BLE2902());
    pCtrl->setCallbacks(&bleCallbacks);
    
    pFtms->start();
    BLEAdvertising *pAdv = BLEDevice::getAdvertising();
    pAdv->addServiceUUID(FTMS_SERVICE_UUID); 
    pAdv->setScanResponse(true); pAdv->setMinPreferred(0x06); pAdv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
}

void BLE_FTMS::updateAndNotify() {
    if (!bleCallbacks.connected) return;
    
    if (millis() - metrics.lastStrokeTime > 4000) { 
        metrics.watts = 0; metrics.spm = 0; metrics.paceSeconds = 0; 
    }

    uint16_t elap = (metrics.cumulativeStrokes > 0) ? (millis() - metrics.workoutStartMs) / 1000 : 0;
    uint16_t kH = (elap > 0) ? (uint16_t)((metrics.totalKcal / elap) * 3600.0) : 0;
    
    uint8_t payload[20];
    payload[0] = 0x2C; payload[1] = 0x0B; 
    payload[2] = (metrics.spm * 2) > 255 ? 255 : (uint8_t)(metrics.spm * 2);
    payload[3] = metrics.cumulativeStrokes & 0xFF; payload[4] = (metrics.cumulativeStrokes >> 8) & 0xFF;
    uint32_t d = (uint32_t)metrics.totalDistance;
    payload[5] = d & 0xFF; payload[6] = (d >> 8) & 0xFF; payload[7] = (d >> 16) & 0xFF;
    payload[8] = metrics.paceSeconds & 0xFF; payload[9] = (metrics.paceSeconds >> 8) & 0xFF;
    payload[10] = metrics.watts & 0xFF; payload[11] = (metrics.watts >> 8) & 0xFF;
    uint16_t tK = (uint16_t)metrics.totalKcal;
    payload[12] = tK & 0xFF; payload[13] = (tK >> 8) & 0xFF;
    payload[14] = kH & 0xFF; payload[15] = (kH >> 8) & 0xFF;
    payload[16] = (uint8_t)(kH / 60);
    payload[17] = elap & 0xFF; payload[18] = (elap >> 8) & 0xFF;
    payload[19] = telemetry.heartRate;

    pData->setValue(payload, 20); pData->notify();
}
#endif