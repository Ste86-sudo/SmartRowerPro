#include "GhostPhysics.h"

GhostPhysics ghost;

void GhostPhysics::buildTemplate() {
    float dt_g = t_off_gambe - t_on_gambe;
    float dt_t = t_off_tronco - t_on_tronco;
    float dt_b = t_off_braccia - t_on_braccia;

    I3 = 0.0f;

    for (int i = 0; i < GHOST_LUT_SIZE; i++) {
        float tau = (float)i / (GHOST_LUT_SIZE - 1);

        float pg = clamp01((tau - t_on_gambe) / dt_g);
        float pt = clamp01((tau - t_on_tronco) / dt_t);
        float pb = clamp01((tau - t_on_braccia) / dt_b);

        float v_hat = 0.0f;
        float a_hat = 0.0f;

        if (pg > 0.0f && pg < 1.0f) {
            v_hat += f_gambe * S_prime(pg) / dt_g;
            a_hat += f_gambe * S_second(pg) / (dt_g * dt_g);
        }
        if (pt > 0.0f && pt < 1.0f) {
            v_hat += f_tronco * S_prime(pt) / dt_t;
            a_hat += f_tronco * S_second(pt) / (dt_t * dt_t);
        }
        if (pb > 0.0f && pb < 1.0f) {
            v_hat += f_braccia * S_prime(pb) / dt_b;
            a_hat += f_braccia * S_second(pb) / (dt_b * dt_b);
        }

        psi[i] = v_hat;
        dpsi[i] = a_hat;
    }

    // Integrale di psi^3 (trapezi) per la potenza teorica
    float dtau = 1.0f / (GHOST_LUT_SIZE - 1);
    for (int i = 0; i < GHOST_LUT_SIZE - 1; i++) {
        float v_avg = (psi[i] + psi[i + 1]) * 0.5f;
        I3 += (v_avg * v_avg * v_avg) * dtau;
    }
}

void GhostPhysics::updateCurves(float R_spm, float d_ratio, float L_drive, float b2, float Meff) {
    if (R_spm < 10) R_spm = 10;

    float T = 60.0f / R_spm;
    float t_drive = d_ratio * T;

    float dt_g = t_off_gambe - t_on_gambe;
    float dt_b = t_off_braccia - t_on_braccia;

    // Calcola le nuove curve in buffer locali, poi pubblica sotto lock:
    // la sezione critica dura solo il tempo di 3 memcpy.
    float newF[GHOST_LUT_SIZE], newVs[GHOST_LUT_SIZE], newVa[GHOST_LUT_SIZE];
    float newPeak = 0.0f;
    float newP = (R_spm / 60.0f) * b2 * L_drive * L_drive * L_drive * I3 / (t_drive * t_drive);

    for (int i = 0; i < GHOST_LUT_SIZE; i++) {
        float tau = (float)i / (GHOST_LUT_SIZE - 1);

        float v_h = (L_drive / t_drive) * psi[i];
        float a_h = (L_drive / (t_drive * t_drive)) * dpsi[i];

        float f = b2 * v_h * v_h + Meff * a_h;
        if (f < 0.0f) f = 0.0f;
        newF[i] = f;
        if (f > newPeak) newPeak = f;

        float pg = clamp01((tau - t_on_gambe) / dt_g);
        float pb = clamp01((tau - t_on_braccia) / dt_b);

        newVs[i] = (pg > 0.0f && pg < 1.0f)
                     ? (L_drive / t_drive) * f_gambe * S_prime(pg) / dt_g : 0.0f;
        newVa[i] = (pb > 0.0f && pb < 1.0f)
                     ? (L_drive / t_drive) * f_braccia * S_prime(pb) / dt_b : 0.0f;
    }

    portENTER_CRITICAL(&ghostMux);
    memcpy(F_ghost, newF, sizeof(F_ghost));
    memcpy(vs_ghost, newVs, sizeof(vs_ghost));
    memcpy(va_ghost, newVa, sizeof(va_ghost));
    P_teorica = newP;
    F_peak = newPeak;
    portEXIT_CRITICAL(&ghostMux);
}

size_t GhostPhysics::getJSON(char* buf, size_t cap) {
    // Snapshot sotto lock, formattazione fuori
    float locF[GHOST_LUT_SIZE], locVs[GHOST_LUT_SIZE], locVa[GHOST_LUT_SIZE];
    float locP, locPeak;

    portENTER_CRITICAL(&ghostMux);
    memcpy(locF, F_ghost, sizeof(locF));
    memcpy(locVs, vs_ghost, sizeof(locVs));
    memcpy(locVa, va_ghost, sizeof(locVa));
    locP = P_teorica;
    locPeak = F_peak;
    portEXIT_CRITICAL(&ghostMux);

    size_t n = 0;
    n += snprintf(buf + n, cap - n, "{\"P_teorica\":%.1f,\"F_peak\":%.1f,\"F_ghost\":[", locP, locPeak);
    for (int i = 0; i < GHOST_LUT_SIZE && n < cap; i++)
        n += snprintf(buf + n, cap - n, i ? ",%.1f" : "%.1f", locF[i]);
    n += snprintf(buf + n, cap - n, "],\"vs_ghost\":[");
    for (int i = 0; i < GHOST_LUT_SIZE && n < cap; i++)
        n += snprintf(buf + n, cap - n, i ? ",%.2f" : "%.2f", locVs[i]);
    n += snprintf(buf + n, cap - n, "],\"va_ghost\":[");
    for (int i = 0; i < GHOST_LUT_SIZE && n < cap; i++)
        n += snprintf(buf + n, cap - n, i ? ",%.2f" : "%.2f", locVa[i]);
    n += snprintf(buf + n, cap - n, "]}");

    return (n < cap) ? n : cap - 1;
}
