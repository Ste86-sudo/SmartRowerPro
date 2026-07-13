#pragma once
#include <Arduino.h>

// Mutex che protegge la coerenza multi-campo di metrics/telemetry
// (accessi da PhysicsTask core 1, NetTask core 0, callback ESP-NOW/BLE)
extern portMUX_TYPE telemetryMux;

// Metriche di allenamento aggiornate al completamento di ogni colpo
struct RowerMetrics {
    volatile uint16_t watts;
    volatile uint16_t spm;
    volatile uint16_t paceSeconds;
    volatile float totalDistance;
    volatile uint16_t cumulativeStrokes;
    volatile float totalKcal;
    volatile unsigned long lastStrokeTime;
    volatile unsigned long workoutStartMs;
    volatile int32_t rawAdc;
};

// Stato telemetria istantanea (maniglia, laser, HR, encoder)
struct TelemetryData {
    volatile int32_t encoderTicks;
    volatile float seatPositionMeters;
    volatile uint16_t heartRate;
    volatile unsigned long lastPacketTime;
    volatile bool active;                 // maniglia connessa (pacchetti < 500 ms fa)
    volatile float currentForce;          // kg
    volatile bool isPulling;
    volatile float currentCablePosition;  // m
    volatile float remotePeakForce;       // picco forza ricevuto dalla maniglia (kg)
    volatile uint32_t lastSeq;
    volatile uint32_t droppedPackets;
    volatile bool encoderFault;           // auto-diagnosi: encoder fermo durante tirate valide
};

extern RowerMetrics metrics;
extern TelemetryData telemetry;
extern volatile uint32_t syncPacketCounter;
