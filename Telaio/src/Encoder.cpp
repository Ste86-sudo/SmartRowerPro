#include "Encoder.h"
#include "Config.h"

EncoderManager encoderManager;

void EncoderManager::begin() {
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachFullQuad(ENC_PIN_A, ENC_PIN_B);
    encoder.clearCount();
}

int64_t EncoderManager::getCount() {
    return encoder.getCount();
}

void EncoderManager::clearCount() {
    encoder.clearCount();
}
