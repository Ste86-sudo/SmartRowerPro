#pragma once
#include <Arduino.h>
#include <ESP32Encoder.h>

// Encoder quadratura su PCNT hardware (nessun carico CPU per il conteggio)
class EncoderManager {
public:
    void begin();
    int64_t getCount();
    void clearCount();

private:
    ESP32Encoder encoder;
};

extern EncoderManager encoderManager;
