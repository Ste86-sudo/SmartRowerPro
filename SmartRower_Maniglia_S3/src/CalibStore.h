#pragma once
#include <Arduino.h>
#include <Preferences.h>

class CalibStore {
public:
    void begin();
    void saveCalibration(int32_t tare, float scale);
    void saveThresholds(float pullThresh, float relThresh);
    void loadCalibration();

    int32_t getTare() const { return currentTare; }
    float getScale() const { return currentScale; }
    float getPullThresh() const { return pullThresh; }
    float getRelThresh() const { return relThresh; }
    float getUHeight() const { return uHeight; }
    float getUWeight() const { return uWeight; }
    
    void saveProfile(float h, float w);

private:
    Preferences preferences;
    int32_t currentTare = 0;
    float currentScale = 10000.0f;
    float pullThresh = 4.0f;
    float relThresh = 2.0f;
    float uHeight = 180.0f;
    float uWeight = 80.0f;
};
extern CalibStore calibStore;
