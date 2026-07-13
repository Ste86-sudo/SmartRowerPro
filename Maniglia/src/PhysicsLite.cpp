#include "PhysicsLite.h"
#include "CalibStore.h"

PhysicsLite physicsLite;

// Ping-pong buffer per il grafico (stesso formato del telaio: 13 terzine)
static constexpr int kBufferSize = 39;
static float bufferA[kBufferSize];
static float bufferB[kBufferSize];
static float* activeWriteBuffer = bufferA;
static float* readyReadBuffer = nullptr;
static int bufferIdx = 0;

float* PhysicsLite::getReadyBuffer() { return readyReadBuffer; }
void PhysicsLite::clearReadyBuffer() { readyReadBuffer = nullptr; }

void PhysicsLite::begin() {
    engine.begin();
    totalDistance = 0.0f;
    totalKcal = 0.0f;
    cumulativeStrokes = 0;
    watts = 0;
    spm = 0;
    paceSeconds = 0;
}

void PhysicsLite::processForce(float fKg) {
    // Campiona la terzina [forza, 0, 0] ogni 10 ms
    static unsigned long lastBufferTime = 0;
    if (millis() - lastBufferTime >= 10) {
        lastBufferTime = millis();
        activeWriteBuffer[bufferIdx++] = fKg;
        activeWriteBuffer[bufferIdx++] = 0.0f;
        activeWriteBuffer[bufferIdx++] = 0.0f;
        if (bufferIdx >= kBufferSize) {
            if (readyReadBuffer == nullptr) {
                readyReadBuffer = activeWriteBuffer;
                activeWriteBuffer = (activeWriteBuffer == bufferA) ? bufferB : bufferA;
            }
            bufferIdx = 0;
        }
    }

    currentForce = fKg;

    StrokeMetrics result;
    if (engine.update(fKg, calibStore.getPullThresh(), calibStore.getRelThresh(),
                      1.0f, millis(), result)) {
        spm = result.spm;
        watts = result.watts;
        paceSeconds = result.paceSeconds;
        totalDistance += result.dDist;
        totalKcal += result.dKcal;
        cumulativeStrokes++;
    }
}
