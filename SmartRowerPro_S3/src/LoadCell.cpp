#include "LoadCell.h"
#include "SharedData.h"
#include "ConfigManager.h"
#include <SPI.h>

LoadCellManager loadCell;

LoadCellManager::LoadCellManager() : ads(ADS1220_CS_PIN, ADS1220_DRDY_PIN) {}

void LoadCellManager::initAds() {
    ads.init();
    ads.setSPIClockSpeed(1000000);              // (2) 1 MHz invece di 4 MHz: molto più robusto all'EMI, costo trascurabile a 600/330 SPS
    ads.setCompareChannels(ADS1220_MUX_1_2);
    ads.setVRefSource(ADS1220_VREF_AVDD_AVSS);
    ads.setGain(ADS1220_GAIN_128);
    ads.setDataRate(ADS1220_DR_LVL_4);          // (3) 330 SPS invece di 600: maggiore reiezione del rumore, sufficiente per il canottaggio
    ads.setConversionMode(ADS1220_CONTINUOUS);
    ads.start();
    // (1) CRITICO: evita il while(digitalRead(DRDY)==HIGH) senza timeout nella libreria,
    //     che sotto EMI manda in hang il task e causa il reset watchdog.
    //     Sicuro: getRaw() legge solo dopo aver verificato DRDY == LOW.
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
        // Forza CS HIGH prima del re-init: se SPI era bloccato (CS stuck low)
        // il chip non risponde ai comandi — questa sequenza lo sblocca.
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
        // Filtra errori di bus SPI (MISO stuck high o low a causa di EMI)
        if (raw == 0 || raw == -1) {
            return metrics.rawAdc; 
        }
        isNew = true;
        return raw;
    }

    // ADS1220 a 600 SPS: DRDY dovrebbe andare LOW ogni ~1.7ms.
    // Oltre 500ms senza dati = chip bloccato (EMI/SPI glitch).
    if (millis() - lastDataTime > 500) {
        needInit = true;
    }

    return metrics.rawAdc;
}

float LoadCellManager::getKg(bool &isNew) {
    int32_t raw = getRaw(isNew);
    if (isNew) {
        metrics.rawAdc = raw;
    }
    return fabs((float)(raw - config.tara)) / config.scala;
}
