#pragma once
#include <Arduino.h>
#include <ADS1220_WE.h>
#include "Config.h"

class LoadCellManager {
public:
    LoadCellManager();
    void begin();
    int32_t getRaw(bool &isNew);
    float getKg(bool &isNew);
    int32_t getLastRaw() const { return lastRaw; }
private:
    ADS1220_WE ads;
    int32_t lastRaw = 0;
    void initAds();
};

extern LoadCellManager loadCell;