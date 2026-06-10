#pragma once
#include <stdint.h>

#define RP_CHANNEL 6

#define PKT_MAGIC_FORCE 0xA1
#define PKT_MAGIC_CMD   0xC1
#define PKT_HEARTBEAT   0x48

#define CMD_TARE        1
#define CMD_CALIB       2

typedef struct __attribute__((packed)) {
    uint8_t  magic;       
    uint32_t seq;         
    int32_t  rawAdc;      
    int16_t  forceCenti;  
    int16_t  peakCenti;   
    uint8_t  flags;       
} ForcePacket;

static_assert(sizeof(ForcePacket) == 14, "ForcePacket size mismatch");
