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
private:
    ADS1220_WE ads;
    void initAds();
};

extern LoadCellManager loadCell;