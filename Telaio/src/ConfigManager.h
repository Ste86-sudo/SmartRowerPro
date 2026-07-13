#pragma once
#include <Preferences.h>

// Persistenza NVS (namespace "rower" — chiavi identiche a SmartRower 1.0)
class ConfigManager {
public:
    int32_t tara;
    float scala;
    float uHeight;
    float uWeight;
    float pullThresh;
    float relThresh;
    float encPPR;
    float pullCirc;      // diametro puleggia in mm (nome storico)
    float laserOffset;
    float uFtp;
    float ghostB2;
    float ghostMeff;
    float ghostLdrive;
    float ghostDR;
    bool encoderEnabled;   // false: metriche stimate dal picco forza (encoder ignorato)
    String wifiSSID;
    String wifiPass;

    void begin();
    void saveTara(int32_t t);
    void saveScale(float s);
    void saveMechanics(float ppr, float circ, float lOffset);
    void saveProfile(float h, float w, float p, float r);
    void saveThresholds(float p, float r);
    void saveWiFi(const String& ssid, const String& pass);
    void saveFtp(float f);
    void saveGhost(float b2, float meff, float ldrive, float dr);
    void saveEncoderMode(bool enabled);

    void savePB(uint16_t dist, uint32_t time);
    uint32_t getPB(uint16_t dist);

    float getMetersPerTick() const { return cachedMetersPerTick; }

private:
    float cachedMetersPerTick = 0.0f;
    void updateCache();
    static void sanitizeThresholds(float& p, float& r);
    Preferences prefs;
};

extern ConfigManager config;
