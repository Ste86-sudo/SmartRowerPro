#pragma once
#include <atomic>
#include <cstddef>

constexpr size_t TELEMETRY_BUFFER_SIZE = 39; // 13 terzine: aggiornamento a ~25Hz con SPS 330

class Physics {
public:
    static void begin();
    static float* getReadyBuffer();
    static void clearReadyBuffer();

private:
    static void taskLoop(void * pvParameters);
    static void processStrokeComplete(float workJoules, float peakForce);
};