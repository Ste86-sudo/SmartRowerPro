#include "ConfigManager.h"

ConfigManager config;

void ConfigManager::begin() {
    prefs.begin("rower", false);
    tara = prefs.getInt("tare", 0);
    scala = prefs.getFloat("scale", 10000.0);
    uHeight = prefs.getFloat("uHeight", 181.0);
    uWeight = prefs.getFloat("uWeight", 75.0);
    pullThresh = prefs.getFloat("uPull", 3.0);
    relThresh = prefs.getFloat("uRel", 1.5);
    encPPR = prefs.getFloat("encPPR", 600.0);
    pullCirc = prefs.getFloat("pullCirc", 150.0);
    laserOffset = prefs.getFloat("lOffset", 0.0);
    wifiSSID = prefs.getString("wifiSsid", "");
    wifiPass = prefs.getString("wifiPass", "");
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