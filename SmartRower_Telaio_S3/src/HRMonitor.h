#pragma once
#include <Arduino.h>

class HRMonitor {
public:
    void begin();
    void loop();
};
extern HRMonitor hrMonitor;
