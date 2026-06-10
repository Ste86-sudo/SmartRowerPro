#include "LoadCell.h"
#include "CalibStore.h"
#include <SPI.h>

LoadCellManager loadCell;

LoadCellManager::LoadCellManager() : ads(ADS1220_CS_PIN, ADS1220_DRDY_PIN) {}

void LoadCellManager::initAds() {
    ads.init();
    ads.setSPIClockSpeed(1000000);              
    ads.setCompareChannels(ADS1220_MUX_1_2);
    ads.setVRefSource(ADS1220_VREF_AVDD_AVSS);
    ads.setGain(ADS1220_GAIN_128);
    ads.setDataRate(ADS1220_DR_LVL_4);          
    ads.setConversionMode(ADS1220_CONTINUOUS);
    ads.start();
    ads.setNonBlockingMode(true);               
}

void LoadCellManager::begin() {
    pinMode(ADS1220_CS_PIN, OUTPUT);
    digitalWrite(ADS1220_CS_PIN, HIGH);
    pinMode(ADS1220_DRDY_PIN, INPUT_PULLUP);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    delay(10);

    initAds();
}

int32_t LoadCellManager::getRaw(bool &isNew) {
    static unsigned long lastDataTime = millis();
    static bool needInit = false;
    isNew = false;

    if (needInit) {
        digitalWrite(ADS1220_CS_PIN, HIGH);
        delay(5);
        initAds();
        needInit = false;
        lastDataTime = millis();
        Serial.println("[LOADCELL] ADS1220 re-inizializzato (timeout DRDY)");
    }

    if (digitalRead(ADS1220_DRDY_PIN) == LOW) {
        lastDataTime = millis();
        int32_t raw = ads.getRawData();
        if (raw == 0 || raw == -1) {
            return lastRaw; 
        }
        isNew = true;
        lastRaw = raw;
        return raw;
    }

    if (millis() - lastDataTime > 500) {
        needInit = true;
    }

    return lastRaw;
}

float LoadCellManager::getKg(bool &isNew) {
    int32_t raw = getRaw(isNew);
    float scale = calibStore.getScale();
    if (fabsf(scale) < 1e-3f) return 0.0f;          // protezione divisione per zero (vedi F5)
    float kg = (float)(raw - calibStore.getTare()) / scale;
    // La cella misura solo trazione: i valori sotto-tara (negativi) sono rumore → clamp a 0.
    // Così si conserva il segno reale invece di rettificarlo con fabs().
    return (kg > 0.0f) ? kg : 0.0f;
}
