#include "PhysicsLite.h"

PhysicsLite physicsLite;

static float bufferA[39];
static float bufferB[39];
static float* activeWriteBuffer = bufferA;
static float* readyReadBuffer = nullptr;
static int bufferIdx = 0;

float* PhysicsLite::getReadyBuffer() { return readyReadBuffer; }
void PhysicsLite::clearReadyBuffer() { readyReadBuffer = nullptr; }

void PhysicsLite::begin() {
    isPulling = false;
    currentPeakForce = 0.0f;
    lastStrokeTime = 0;
    totalDistance = 0.0f;
    totalKcal = 0.0f;
    cumulativeStrokes = 0;
}



void PhysicsLite::processForce(float fKg) {
    float pullThresh = 4.0f;
    float relThresh = 2.0f;

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

    if (fKg > pullThresh) {
        if (!isPulling) { 
            isPulling = true; 
            currentPeakForce = 0.0f; 
        }
        if (fKg > currentPeakForce) {
            currentPeakForce = fKg;
        }
    }
    else if (fKg < relThresh && isPulling) {
        isPulling = false;
        
        float strokeLengthMeters = 1.0f; // Stima empirica (1m)
        float workJoules = (currentPeakForce * 0.6f * 9.81f) * strokeLengthMeters;
        
        unsigned long now = millis();
        if (lastStrokeTime > 0) {
            float dt = (now - lastStrokeTime) / 1000.0f;
            if (dt > 0.001f) {
                spm = (uint16_t)(60.0f / dt);
                watts = (uint16_t)(workJoules / dt);
                
                float s = (watts > 0) ? powf((float)watts / 2.8f, 1.0f/3.0f) : 0.0f;
                paceSeconds = (s > 0.0f) ? (500.0f / s) : 0;

                totalDistance += s * dt;
                totalKcal += (workJoules / 4184.0f) * 4.0f;
                cumulativeStrokes++;
            }
        }
        lastStrokeTime = now;
    }
}
