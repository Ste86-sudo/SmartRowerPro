#pragma once
#include <Arduino.h>

#define GHOST_LUT_SIZE 64

// Modello del "vogatore fantasma": curve teoriche forza/velocità
// ricalcolate ad ogni colpo in funzione dello SPM corrente.
class GhostPhysics {
public:
    // Frazioni e fasi della sequenza gambe/tronco/braccia (drive normalizzato)
    float f_gambe = 0.33f;
    float f_tronco = 0.31f;
    float f_braccia = 0.36f;

    float t_on_gambe = 0.0f,    t_off_gambe = 0.60f;
    float t_on_tronco = 0.30f,  t_off_tronco = 0.85f;
    float t_on_braccia = 0.55f, t_off_braccia = 1.00f;

    void buildTemplate();
    void updateCurves(float R_spm, float d_ratio, float L_drive, float b2, float Meff);

    // Serializza le curve in JSON nel buffer fornito; ritorna i byte scritti.
    // Lo snapshot dei dati avviene sotto lock, la formattazione fuori:
    // niente allocazioni heap con gli interrupt disabilitati.
    size_t getJSON(char* buf, size_t cap);

private:
    // Template normalizzato (scritto solo da buildTemplate al boot)
    float psi[GHOST_LUT_SIZE];
    float dpsi[GHOST_LUT_SIZE];
    float I3 = 0.0f;

    // LUT di output (protette da ghostMux)
    portMUX_TYPE ghostMux = portMUX_INITIALIZER_UNLOCKED;
    float F_ghost[GHOST_LUT_SIZE];
    float vs_ghost[GHOST_LUT_SIZE];
    float va_ghost[GHOST_LUT_SIZE];
    float P_teorica = 0.0f;
    float F_peak = 0.0f;

    static float S_prime(float p) { return 30.0f * p * p * (1.0f - p) * (1.0f - p); }
    static float S_second(float p) { return 60.0f * p * (1.0f - p) * (1.0f - 2.0f * p); }
    static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
};

extern GhostPhysics ghost;
