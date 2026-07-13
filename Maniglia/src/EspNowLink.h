#pragma once
#include <stdint.h>
#include "RowerProtocol.h"

// Link ESP-NOW verso il telaio: pairing su heartbeat, TX forza a 100 Hz,
// ricezione comandi di calibrazione (estratto da main.cpp di 1.0)
class EspNowLink {
public:
    bool begin();                                  // init radio + peer broadcast
    void lockChannel();                            // ri-forza il canale RP_CHANNEL

    bool heartbeatReceived() const;
    bool isPaired() const;

    // Ritorna true se c'è un comando pendente e lo consuma
    bool fetchCommand(uint8_t& cmd, float& param);

    // Invia un campione di forza al telaio (seq gestito internamente)
    void sendForce(int32_t rawAdc, int16_t forceCenti, int16_t peakCenti);
};

extern EspNowLink espNowLink;
