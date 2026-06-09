#include "ConfigManager.h"

ConfigManager config;

void ConfigManager::begin() {
    prefs.begin("rower", false);
    tara = prefs.getInt("tare", 0);
    scala = prefs.getFloat("scale", 10000.0);
    uHeight = prefs.getFloat("uHeight", 181.0);
    uWeight = prefs.getFloat("uWeight", 75.0);
    pullThresh = prefs.getFloat("uPull", 3.0f);
    relThresh = prefs.getFloat("uRel", 1.5f);
    encPPR = prefs.getFloat("encPPR", 600.0f);
    pullCirc = prefs.getFloat("pullCirc", 100.0f);
    laserOffset = prefs.getFloat("lOffset", 0.0f);
    uFtp = prefs.getFloat("uFtp", 200.0f);

    if (isnan(pullThresh) || pullThresh <= 0.0f || pullThresh > 50.0f) pullThresh = 3.0f;
    if (isnan(relThresh) || relThresh <= 0.0f || relThresh > 50.0f) relThresh = 1.5f;
    if (isnan(encPPR) || encPPR <= 0.001f) encPPR = 600.0f;
    if (isnan(pullCirc) || pullCirc <= 0.001f) pullCirc = 100.0f;
    if (isnan(uFtp) || uFtp <= 0.0f) uFtp = 200.0f;
    wifiSSID = prefs.getString("wifiSsid", "");
    wifiPass = prefs.getString("wifiPass", "");
    updateCache();
}

void ConfigManager::saveTara(int32_t t) { 
    tara = t; prefs.putInt("tare", t); 
}
void ConfigManager::saveScale(float s) { 
    scala = s; prefs.putFloat("scale", s); 
}
void ConfigManager::saveMechanics(float ppr, float circ, float lOffset) { 
    encPPR = ppr; pullCirc = circ; laserOffset = lOffset;
    prefs.putFloat("encPPR", ppr); prefs.putFloat("pullCirc", circ); prefs.putFloat("lOffset", lOffset);
    updateCache();
}
void ConfigManager::saveWiFi(String ssid, String pass) {
    wifiSSID = ssid; wifiPass = pass;
    prefs.putString("wifiSsid", ssid); prefs.putString("wifiPass", pass);
}
void ConfigManager::saveProfile(float h, float w, float p, float r) {
    uHeight = h; uWeight = w; pullThresh = p; relThresh = r;
    prefs.putFloat("uHeight", h); prefs.putFloat("uWeight", w); 
    prefs.putFloat("uPull", p); prefs.putFloat("uRel", r);
}

void ConfigManager::savePB(uint16_t dist, uint32_t time) {
    String key = "pb_" + String(dist);
    prefs.putUInt(key.c_str(), time);
}

uint32_t ConfigManager::getPB(uint16_t dist) {
    String key = "pb_" + String(dist);
    return prefs.getUInt(key.c_str(), 0);
}

float ConfigManager::getMetersPerTick() {
    return cachedMetersPerTick;
}

void ConfigManager::updateCache() {
    float pCirc = pullCirc;
    float ePPR = encPPR;
    if (pCirc <= 0.001f || isnan(pCirc) || isinf(pCirc)) pCirc = 100.0f;
    if (ePPR <= 0.001f || isnan(ePPR) || isinf(ePPR)) ePPR = 600.0f;
    cachedMetersPerTick = (pCirc / 1000.0f) / ePPR;
}