#pragma once
#include <Arduino.h>

class PhysicsLite {
public:
    void begin();
    void processForce(float kg);

    float getCurrentPace() const { return paceSeconds; }
    uint16_t getSpm() const { return spm; }
    uint16_t getWatts() const { return watts; }

    float getTotalDistance() const { return totalDistance; }
    float getTotalKcal() const { return totalKcal; }
    uint16_t getStrokes() const { return cumulativeStrokes; }
    bool getIsPulling() const { return isPulling; }
    float getCurrentForce() const { return currentForce; }
    
    float* getReadyBuffer();
    void clearReadyBuffer();

private:
    float currentForce = 0.0f;
    float currentPeakForce = 0.0f;
    bool isPulling = false;
    unsigned long lastStrokeTime = 0;

    uint16_t spm = 0;
    uint16_t watts = 0;
    float paceSeconds = 0.0f;
    float totalDistance = 0.0f;
    float totalKcal = 0.0f;
    uint16_t cumulativeStrokes = 0;
};

extern PhysicsLite physicsLite;
