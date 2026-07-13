#include "ConfigManager.h"
#include <math.h>

ConfigManager config;

void ConfigManager::sanitizeThresholds(float& p, float& r) {
    if (isnan(p) || p <= 0.01f || p > 200.0f) p = 3.0f;
    if (isnan(r) || r <= 0.01f || r > 200.0f) r = 1.5f;
    if (r >= p) r = p * 0.5f;
}

void ConfigManager::begin() {
    prefs.begin("rower", false);
    tara = prefs.getInt("tare", 0);
    scala = prefs.getFloat("scale", 10000.0f);
    uHeight = prefs.getFloat("uHeight", 181.0f);
    uWeight = prefs.getFloat("uWeight", 75.0f);
    pullThresh = prefs.getFloat("uPull", 3.0f);
    relThresh = prefs.getFloat("uRel", 1.5f);
    encPPR = prefs.getFloat("encPPR", 600.0f);
    pullCirc = prefs.getFloat("pullCirc", 100.0f);
    laserOffset = prefs.getFloat("lOffset", 0.0f);
    uFtp = prefs.getFloat("uFtp", 200.0f);

    sanitizeThresholds(pullThresh, relThresh);
    if (isnan(encPPR) || encPPR <= 0.001f) encPPR = 600.0f;
    if (isnan(pullCirc) || pullCirc <= 0.001f) pullCirc = 100.0f;
    if (isnan(uFtp) || uFtp <= 0.0f) uFtp = 200.0f;

    ghostB2 = prefs.getFloat("gB2", 110.0f);
    ghostMeff = prefs.getFloat("gMeff", 0.0f);
    ghostLdrive = prefs.getFloat("gLdrive", 1.25f);
    ghostDR = prefs.getFloat("gDR", 0.33f);
    if (isnan(ghostB2) || ghostB2 <= 0.0f) ghostB2 = 110.0f;
    if (isnan(ghostLdrive) || ghostLdrive <= 0.1f) ghostLdrive = 1.25f;
    if (isnan(ghostDR) || ghostDR <= 0.0f || ghostDR > 1.0f) ghostDR = 0.33f;

    encoderEnabled = prefs.getBool("encEn", true);

    wifiSSID = prefs.getString("wifiSsid", "");
    wifiPass = prefs.getString("wifiPass", "");
    updateCache();
}

void ConfigManager::saveTara(int32_t t) {
    tara = t;
    prefs.putInt("tare", t);
}

void ConfigManager::saveScale(float s) {
    scala = s;
    prefs.putFloat("scale", s);
}

void ConfigManager::saveMechanics(float ppr, float circ, float lOffset) {
    if (isnan(ppr) || ppr <= 0.001f) ppr = 600.0f;
    if (isnan(circ) || circ <= 0.001f) circ = 100.0f;
    encPPR = ppr;
    pullCirc = circ;
    laserOffset = lOffset;
    prefs.putFloat("encPPR", ppr);
    prefs.putFloat("pullCirc", circ);
    prefs.putFloat("lOffset", lOffset);
    updateCache();
}

void ConfigManager::saveWiFi(const String& ssid, const String& pass) {
    wifiSSID = ssid;
    wifiPass = pass;
    prefs.putString("wifiSsid", ssid);
    prefs.putString("wifiPass", pass);
}

void ConfigManager::saveProfile(float h, float w, float p, float r) {
    sanitizeThresholds(p, r);
    uHeight = h;
    uWeight = w;
    pullThresh = p;
    relThresh = r;
    prefs.putFloat("uHeight", h);
    prefs.putFloat("uWeight", w);
    prefs.putFloat("uPull", p);
    prefs.putFloat("uRel", r);
}

void ConfigManager::saveThresholds(float p, float r) {
    sanitizeThresholds(p, r);
    pullThresh = p;
    relThresh = r;
    prefs.putFloat("uPull", p);
    prefs.putFloat("uRel", r);
}

void ConfigManager::saveFtp(float f) {
    if (isnan(f) || f <= 0.0f) f = 200.0f;
    uFtp = f;
    prefs.putFloat("uFtp", f);
}

void ConfigManager::saveGhost(float b2, float meff, float ldrive, float dr) {
    if (isnan(b2) || b2 <= 0.0f) b2 = 110.0f;
    if (isnan(ldrive) || ldrive <= 0.1f) ldrive = 1.25f;
    if (isnan(dr) || dr <= 0.0f || dr > 1.0f) dr = 0.33f;
    ghostB2 = b2;
    ghostMeff = meff;
    ghostLdrive = ldrive;
    ghostDR = dr;
    prefs.putFloat("gB2", b2);
    prefs.putFloat("gMeff", meff);
    prefs.putFloat("gLdrive", ldrive);
    prefs.putFloat("gDR", dr);
}

void ConfigManager::saveEncoderMode(bool enabled) {
    encoderEnabled = enabled;
    prefs.putBool("encEn", enabled);
}

void ConfigManager::savePB(uint16_t dist, uint32_t time) {
    char key[16];
    snprintf(key, sizeof(key), "pb_%u", dist);
    prefs.putUInt(key, time);
}

uint32_t ConfigManager::getPB(uint16_t dist) {
    char key[16];
    snprintf(key, sizeof(key), "pb_%u", dist);
    return prefs.getUInt(key, 0);
}

void ConfigManager::updateCache() {
    float pCirc = pullCirc;
    float ePPR = encPPR;
    if (pCirc <= 0.001f || isnan(pCirc) || isinf(pCirc)) pCirc = 100.0f;
    if (ePPR <= 0.001f || isnan(ePPR) || isinf(ePPR)) ePPR = 600.0f;

    // pullCirc a UI è il diametro in mm: circonferenza = diametro * PI
    cachedMetersPerTick = ((pCirc * 3.14159265f) / 1000.0f) / ePPR;
}
