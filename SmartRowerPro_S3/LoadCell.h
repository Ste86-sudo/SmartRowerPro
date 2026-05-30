#pragma once
#include <Arduino.h>
#include <ADS1220_WE.h>
#include "Config.h"

class LoadCellManager {
public:
    LoadCellManager();
    void begin();
    int32_t getRaw();
    float getKg();
private:
    ADS1220_WE ads;
};

extern LoadCellManager loadCell;