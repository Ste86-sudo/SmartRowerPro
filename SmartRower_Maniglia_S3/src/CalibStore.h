#pragma once
#include <Arduino.h>
#include <Preferences.h>

class CalibStore {
public:
    void begin();
    void saveCalibration(int32_t tare, float scale);
    void loadCalibration();

    int32_t getTare() const { return currentTare; }
    float getScale() const { return currentScale; }

private:
    Preferences preferences;
    int32_t currentTare = 0;
    float currentScale = 10000.0f;
};
extern CalibStore calibStore;
