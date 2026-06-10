#pragma once
#include <Arduino.h>
#include "StrokeEngine.h"

class PhysicsLite {
public:
    void begin();
    void processForce(float fKg);

    uint16_t getWatts() const { return watts; }
    uint16_t getSpm() const { return spm; }
    float getCurrentPace() const { return paceSeconds; }
    float getTotalDistance() const { return totalDistance; }
    float getTotalKcal() const { return totalKcal; }
    bool getIsPulling() const { return engine.getIsPulling(); }
    float getCurrentForce() const { return currentForce; }
    
    float* getReadyBuffer();
    void clearReadyBuffer();

private:
    StrokeEngine engine;

    uint16_t watts = 0;
    uint16_t spm = 0;
    uint16_t paceSeconds = 0;
    float totalDistance = 0.0f;
    float totalKcal = 0.0f;
    uint16_t cumulativeStrokes = 0;
    float currentForce = 0.0f;
};

extern PhysicsLite physicsLite;
