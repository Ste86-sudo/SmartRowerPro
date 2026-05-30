#pragma once
#include "Config.h"

#if ENABLE_BLE
class BLE_FTMS {
public:
    void begin();
    void updateAndNotify();
};

extern BLE_FTMS bleManager;
#endif