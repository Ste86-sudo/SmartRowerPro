#include "CalibStore.h"
#include <math.h>

CalibStore calibStore;

void CalibStore::begin() {
    preferences.begin("rower_calib", false);
    loadCalibration();
}

void CalibStore::loadCalibration() {
    currentTare = preferences.getLong("tare", 0);
    currentScale = preferences.getFloat("scale", 10000.0f);
    pullThresh = preferences.getFloat("uPull", 4.0f);
    relThresh = preferences.getFloat("uRel", 2.0f);
    uHeight = preferences.getFloat("uHeight", 180.0f);
    uWeight = preferences.getFloat("uWeight", 80.0f);
    
    if (isnan(pullThresh) || pullThresh <= 0.01f || pullThresh > 200.0f) pullThresh = 4.0f;
    if (isnan(relThresh) || relThresh <= 0.01f || relThresh > 200.0f) relThresh = 2.0f;

    Serial.printf("[CalibStore] Caricato: Tara=%ld, Scala=%.2f, Soglie=%.1f/%.1f\n", currentTare, currentScale, pullThresh, relThresh);
}

void CalibStore::saveCalibration(int32_t tare, float scale) {
    if (fabsf(scale) < 1e-3f) {                     // scale ~0 → rifiuta, eviterebbe div/0 in getKg
        Serial.println("[CalibStore] Scala non valida (~0), salvataggio annullato");
        return;
    }
    currentTare = tare;
    currentScale = scale;
    preferences.putLong("tare", tare);
    preferences.putFloat("scale", scale);
    Serial.printf("[CalibStore] Salvato: Tara=%ld, Scala=%.2f\n", tare, scale);
}

void CalibStore::saveThresholds(float pull, float rel) {
    if (pull <= 0.01f || pull > 200.0f) pull = 4.0f;
    if (rel <= 0.01f || rel > 200.0f) rel = 2.0f;
    if (rel >= pull) rel = pull * 0.5f;

    pullThresh = pull;
    relThresh = rel;
    preferences.putFloat("uPull", pull);
    preferences.putFloat("uRel", rel);
    Serial.printf("[CalibStore] Soglie salvate: Pull=%.1f, Rel=%.1f\n", pull, rel);
}

void CalibStore::saveProfile(float h, float w) {
    uHeight = h;
    uWeight = w;
    preferences.putFloat("uHeight", h);
    preferences.putFloat("uWeight", w);
    Serial.printf("[CalibStore] Profilo salvato: H=%.1f, W=%.1f\n", h, w);
}
