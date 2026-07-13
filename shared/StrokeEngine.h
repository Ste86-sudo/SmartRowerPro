#pragma once
#include "StrokeMath.h"

// Macchina a stati pull/release con metriche stimate dal picco di forza.
// Usata dalla Maniglia in standalone e dal Telaio come fallback quando
// la telemetria ESP-NOW non è attiva.
class StrokeEngine {
public:
    void begin() {
        pulling = false;
        peak = 0.0f;
        lastStrokeMs = 0;
        math.reset();
    }

    // Ritorna true se un colpo valido è stato completato (out popolato).
    // overridePeak > 0 usa quel picco al posto di quello rilevato localmente.
    bool update(float forceKg, float pullThresh, float relThresh, float strokeLengthM,
                uint32_t nowMs, StrokeMetrics& out, float overridePeak = -1.0f) {
        if (forceKg > pullThresh) {
            if (!pulling) {
                pulling = true;
                peak = 0.0f;
            }
            if (forceKg > peak) peak = forceKg;
            return false;
        }

        if (forceKg >= relThresh || !pulling) return false;

        // Rilascio: fine tirata
        pulling = false;

        if (lastStrokeMs == 0) {
            lastStrokeMs = nowMs;
            return false;
        }

        float dt = (nowMs - lastStrokeMs) / 1000.0f;
        if (dt < RowerModel::kMinStrokeSec) return false;   // debounce

        float peakToUse = (overridePeak > 0.0f) ? overridePeak : peak;
        math.compute(RowerModel::estimateWork(peakToUse, strokeLengthM), dt, out);
        lastStrokeMs = nowMs;
        return true;
    }

    bool isPulling() const { return pulling; }
    float peakForce() const { return peak; }

private:
    StrokeMath math;
    bool pulling = false;
    float peak = 0.0f;
    uint32_t lastStrokeMs = 0;
};
