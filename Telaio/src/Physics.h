#pragma once
#include <atomic>
#include <cstddef>

// 13 terzine [forza, posizione cavo, posizione carrello]:
// 1 terzina/10 ms -> flush ogni ~130 ms (~7.7 Hz) verso la web UI
constexpr size_t TELEMETRY_BUFFER_SIZE = 39;

class Physics {
public:
    static void begin();
    static float* getReadyBuffer();
    static void clearReadyBuffer();

private:
    static void taskLoop(void* pvParameters);
    static void processStrokeComplete(float workJoules);
};
