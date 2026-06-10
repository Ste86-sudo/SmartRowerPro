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
    Serial.printf("[CalibStore] Caricato: Tara=%ld, Scala=%.2f\n", currentTare, currentScale);
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
