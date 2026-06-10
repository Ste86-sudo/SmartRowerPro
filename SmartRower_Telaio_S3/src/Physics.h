#pragma once
#include <atomic>
#include <cstddef>

constexpr size_t TELEMETRY_BUFFER_SIZE = 39; // 13 terzine: 1 terzina/10ms -> flush ogni ~130 ms (~7.7 Hz)

class Physics {
public:
    static void begin();
    static float* getReadyBuffer();
    static void clearReadyBuffer();

private:
    static void taskLoop(void * pvParameters);
    static void processStrokeComplete(float workJoules, float peakForce);
};