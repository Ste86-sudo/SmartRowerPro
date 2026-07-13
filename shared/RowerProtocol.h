#pragma once
#include <stdint.h>
#include <string.h>

// ============================================================
// Protocollo ESP-NOW Telaio <-> Maniglia
// FORMATI WIRE INVARIATI rispetto a SmartRower 1.0:
// nuovi e vecchi firmware restano interoperabili.
// ============================================================

#define RP_CHANNEL 6   // canale radio fisso — DEVE coincidere su entrambi i lati

// Magic byte / tipi pacchetto
#define PKT_MAGIC_FORCE 0xA1   // Maniglia -> Telaio: campione di forza (100 Hz)
#define PKT_MAGIC_CMD   0xC1   // Telaio -> Maniglia: comando calibrazione
#define PKT_HEARTBEAT   0x48   // Telaio -> Maniglia: heartbeat (1 byte, 2 Hz)

// Comandi (payload di CmdPacket)
#define CMD_TARE        1      // esegui tara con la lettura raw corrente
#define CMD_CALIB       2      // calibra: param = peso di riferimento (kg)
#define CMD_SET_TARE    3      // imposta tara: param = raw int32 (bit-cast nel float)
#define CMD_SET_SCALE   4      // imposta scala: param = float

// Maniglia -> Telaio, 14 byte
typedef struct __attribute__((packed)) {
    uint8_t  magic;       // PKT_MAGIC_FORCE
    uint32_t seq;         // contatore progressivo (rileva pacchetti persi)
    int32_t  rawAdc;      // lettura grezza ADS1220
    int16_t  forceCenti;  // forza istantanea in centesimi di kg
    int16_t  peakCenti;   // picco forza nella finestra di trasmissione
    uint8_t  flags;       // riservato
} ForcePacket;

static_assert(sizeof(ForcePacket) == 14, "ForcePacket size mismatch");

// Telaio -> Maniglia, 6 byte: [magic][cmd][param 4 byte]
typedef struct __attribute__((packed)) {
    uint8_t magic;        // PKT_MAGIC_CMD
    uint8_t cmd;          // CMD_*
    uint8_t param[4];     // float o int32 a seconda del comando
} CmdPacket;

static_assert(sizeof(CmdPacket) == 6, "CmdPacket size mismatch");

inline CmdPacket makeCmdPacket(uint8_t cmd, float param) {
    CmdPacket p;
    p.magic = PKT_MAGIC_CMD;
    p.cmd = cmd;
    memcpy(p.param, &param, 4);
    return p;
}

inline CmdPacket makeCmdPacket(uint8_t cmd, int32_t param) {
    CmdPacket p;
    p.magic = PKT_MAGIC_CMD;
    p.cmd = cmd;
    uint32_t u = (uint32_t)param;
    memcpy(p.param, &u, 4);
    return p;
}
