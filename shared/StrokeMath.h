#pragma once
#include <stdint.h>
#include <math.h>

// Metriche prodotte al completamento di un colpo
struct StrokeMetrics {
    uint16_t spm = 0;          // colpi/minuto (media mobile 3 colpi)
    uint16_t watts = 0;
    uint16_t paceSeconds = 0;  // secondi per 500 m
    float dDist = 0.0f;        // metri percorsi in questo colpo
    float dKcal = 0.0f;        // kcal bruciate in questo colpo
};

// Costanti del modello fisico (identiche a SmartRower 1.0)
namespace RowerModel {
    constexpr float kGravity      = 9.81f;
    constexpr float kPaceFactor   = 2.8f;            // Concept2: W = 2.8 * v^3
    constexpr float kKcalPerJoule = 4.0f / 4184.0f;  // rendimento umano ~25%
    constexpr float kMinStrokeSec = 0.7f;            // debounce anti doppio colpo
    constexpr uint16_t kMaxSpm    = 60;
    constexpr uint32_t kMaxWatts  = 1500;

    // Lavoro stimato dal solo picco di forza (modalità senza encoder):
    // forza media ~60% del picco su tutta la corsa.
    inline float estimateWork(float peakKg, float strokeLengthM) {
        return peakKg * 0.6f * kGravity * strokeLengthM;
    }
}

// Matematica del colpo condivisa tra Telaio (lavoro integrato dall'encoder)
// e Maniglia/fallback (lavoro stimato dal picco). Unica implementazione di
// SPM smoothing, watt, pace, distanza e kcal.
class StrokeMath {
public:
    void reset() {
        idx = 0;
        count = 0;
        for (uint8_t i = 0; i < kWindow; i++) history[i] = 0;
    }

    // workJoules = lavoro del colpo, dtSec = durata (già validata > kMinStrokeSec)
    void compute(float workJoules, float dtSec, StrokeMetrics& out) {
        uint16_t rawSpm = (uint16_t)(60.0f / dtSec);
        if (rawSpm > RowerModel::kMaxSpm) rawSpm = RowerModel::kMaxSpm;

        history[idx] = rawSpm;
        idx = (idx + 1) % kWindow;
        if (count < kWindow) count++;

        uint32_t sum = 0;
        for (uint8_t i = 0; i < count; i++) sum += history[i];
        out.spm = (uint16_t)(sum / count);

        uint32_t w = (uint32_t)(workJoules / dtSec);
        if (w > RowerModel::kMaxWatts) w = RowerModel::kMaxWatts;
        out.watts = (uint16_t)w;

        // cbrtf è più veloce di powf(x, 1/3)
        float speed = (out.watts > 0) ? cbrtf((float)out.watts / RowerModel::kPaceFactor) : 0.0f;
        out.paceSeconds = (speed > 0.0f) ? (uint16_t)(500.0f / speed) : 0;
        out.dDist = speed * dtSec;
        out.dKcal = workJoules * RowerModel::kKcalPerJoule;
    }

private:
    static constexpr uint8_t kWindow = 3;
    uint16_t history[kWindow] = {0};
    uint8_t idx = 0;
    uint8_t count = 0;
};
