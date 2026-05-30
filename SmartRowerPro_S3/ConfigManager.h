#pragma once
#include <Preferences.h>

class ConfigManager {
public:
    float tara;
    float scala;
    float uHeight;
    float uWeight;
    float pullThresh;
    float relThresh;
    float encPPR;
    float pullCirc;
    float laserOffset;
    String wifiSSID;
    String wifiPass;

    void begin();
    void saveTara(int32_t t);
    void saveScale(float s);
    void saveMechanics(float ppr, float circ, float lOffset);
    void saveProfile(float h, float w, float p, float r);
    void saveWiFi(String ssid, String pass);

private:
    Preferences prefs;
};

extern ConfigManager config;