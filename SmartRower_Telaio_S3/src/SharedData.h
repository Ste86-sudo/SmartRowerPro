#pragma once
#include <Arduino.h>

extern portMUX_TYPE telemetryMux;   // protegge la coerenza multi-campo di metrics/telemetry

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

struct TelemetryData {
    volatile int32_t encoderTicks;
    volatile float seatPositionMeters;
    volatile uint16_t heartRate;
    volatile unsigned long lastPacketTime;
    volatile bool active;
    volatile float currentForce;
    volatile bool isPulling;
    volatile float currentCablePosition;
    volatile float remotePeakForce;   // picco forza ricevuto dalla maniglia (kg)
    volatile uint32_t lastSeq;        // ultimo seq ricevuto (per drop-detection futura, Fase 5)
};

extern RowerMetrics metrics;
extern TelemetryData telemetry;
extern volatile uint32_t syncPacketCounter;
