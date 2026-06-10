#pragma once
#include <Preferences.h>

class ConfigManager {
public:
    int32_t tara;
    float scala;
    float uHeight;
    float uWeight;
    float pullThresh;
    float relThresh;
    float encPPR;
    float pullCirc;
    float laserOffset;
    float uFtp;
    String wifiSSID;
    String wifiPass;

    void begin();
    void saveTara(int32_t t);
    void saveScale(float s);
    void saveMechanics(float ppr, float circ, float lOffset);
    void saveProfile(float h, float w, float p, float r);
    void saveWiFi(String ssid, String pass);

    void savePB(uint16_t dist, uint32_t time);
    uint32_t getPB(uint16_t dist);

    float getMetersPerTick();

private:
    float cachedMetersPerTick = 0.0f;
    void updateCache();
    Preferences prefs;
};

extern ConfigManager config;