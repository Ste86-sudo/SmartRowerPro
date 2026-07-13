#pragma once
#include <Arduino.h>
#include <ADS1220_WE.h>
#include "Config.h"

// Cella di carico via ADS1220 (SPI, non bloccante, auto-recovery su stallo DRDY)
class LoadCellManager {
public:
    LoadCellManager();
    void begin();
    int32_t getRaw(bool& isNew);
    float getKg(bool& isNew);
    int32_t getLastRaw() const { return lastRaw; }

private:
    void initAds();

    ADS1220_WE ads;
    int32_t lastRaw = 0;
};

extern LoadCellManager loadCell;
