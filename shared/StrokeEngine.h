#pragma once
#include <stdint.h>
#include <math.h>

struct StrokeResult {
    uint16_t spm;
    uint16_t watts;
    uint16_t paceSeconds;
    float dDist;
    float dKcal;
};

class StrokeEngine {
public:
    inline void begin() {
        isPulling = false;
        currentPeakForce = 0.0f;
        lastStrokeTime = 0;
    }
    
    // Ritorna true se il colpo è completato
    inline bool update(float forceKg, float pullThresh, float relThresh, float strokeLength, unsigned long nowMs, StrokeResult& out, float overridePeakForce = -1.0f) {
        bool finished = false;

        if (forceKg > pullThresh) {
            if (!isPulling) {
                isPulling = true;
                currentPeakForce = 0.0f;
            }
            if (forceKg > currentPeakForce) {
                currentPeakForce = forceKg;
            }
        }
        else if (forceKg < relThresh && isPulling) {
            isPulling = false;
            finished = true;

            float peakToUse = (overridePeakForce > 0.0f) ? overridePeakForce : currentPeakForce;
            float workJoules = (peakToUse * 0.6f * 9.81f) * strokeLength;

            if (lastStrokeTime > 0) {
                float dt = (nowMs - lastStrokeTime) / 1000.0f;
                
                // Fix 8: Debounce dt < 0.7s -> scarta colpo
                if (dt >= 0.7f) {
                    out.spm = (uint16_t)(60.0f / dt);
                    if (out.spm > 60) out.spm = 60; // clamp SPM
                    
                    uint32_t w = (uint32_t)(workJoules / dt);
                    if (w > 1500) w = 1500; // clamp Watts
                    out.watts = (uint16_t)w;
                    
                    float s = (out.watts > 0) ? powf((float)out.watts / 2.8f, 1.0f/3.0f) : 0.0f;
                    out.paceSeconds = (s > 0.0f) ? (uint16_t)(500.0f / s) : 0;
                    
                    out.dDist = s * dt;
                    out.dKcal = (workJoules / 4184.0f) * 4.0f;
                } else {
                    // Scartato per debounce
                    finished = false;
                }
            }
            
            // Non aggiorniamo lastStrokeTime in caso di scarto debounce
            if (finished || lastStrokeTime == 0) {
                lastStrokeTime = nowMs;
            }
        }

        return finished;
    }

    inline bool getIsPulling() const { return isPulling; }
    inline float getCurrentPeakForce() const { return currentPeakForce; }

private:
    bool isPulling = false;
    float currentPeakForce = 0.0f;
    unsigned long lastStrokeTime = 0;
};
