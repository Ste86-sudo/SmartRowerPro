#pragma once
#include <Arduino.h>

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
    volatile uint8_t heartRate;
    volatile unsigned long lastPacketTime;
    volatile bool active;
};

extern RowerMetrics metrics;
extern TelemetryData telemetry;
extern volatile uint32_t syncPacketCounter;