#include "PhysicsLite.h"
#include "CalibStore.h"

PhysicsLite physicsLite;

static float bufferA[39];
static float bufferB[39];
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
    float pullThresh = calibStore.getPullThresh();
    float relThresh = calibStore.getRelThresh();

    static unsigned long lastBufferTime = 0;
    if (millis() - lastBufferTime >= 10) {
        lastBufferTime = millis();
        activeWriteBuffer[bufferIdx++] = fKg;
        activeWriteBuffer[bufferIdx++] = 0.0f;
        activeWriteBuffer[bufferIdx++] = 0.0f;
        if (bufferIdx >= 39) {
            if (readyReadBuffer == nullptr) {
                readyReadBuffer = activeWriteBuffer;
                activeWriteBuffer = (activeWriteBuffer == bufferA) ? bufferB : bufferA;
            }
            bufferIdx = 0;
        }
    }

    currentForce = fKg;

    StrokeResult result;
    bool finished = engine.update(fKg, pullThresh, relThresh, 1.0f, millis(), result);
    if (finished) {
        spm = result.spm;
        watts = result.watts;
        paceSeconds = result.paceSeconds;
        totalDistance += result.dDist;
        totalKcal += result.dKcal;
        cumulativeStrokes++;
    }
}
