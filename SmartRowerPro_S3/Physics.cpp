#include "Physics.h"
#include <Arduino.h>
#include "SharedData.h"
#include "ConfigManager.h"
#include "LoadCell.h"
#include "Telemetry.h"

// Ping-Pong buffer per il grafico
static float bufferA[150];
static float bufferB[150];
static volatile float* activeWriteBuffer = bufferA;
static volatile float* readyReadBuffer = nullptr;
static int bufferIdx = 0;

volatile float* Physics::getReadyBuffer() { return readyReadBuffer; }
void Physics::clearReadyBuffer() { readyReadBuffer = nullptr; }

void Physics::begin() {
    xTaskCreatePinnedToCore(taskLoop, "PhysicsTask", 8192, NULL, 2, NULL, 1);
}

void Physics::processStrokeComplete(float workJoules, float peakForce) {
    unsigned long now = millis();
    if (metrics.lastStrokeTime > 0) {
        if (metrics.cumulativeStrokes == 0) metrics.workoutStartMs = now;
        
        float dt = (now - metrics.lastStrokeTime) / 1000.0;
        metrics.spm = (uint16_t)(60.0 / dt);
        metrics.watts = (uint16_t)(workJoules / dt);
        metrics.totalKcal += ((workJoules / 4184.0) * 4.0);

        float s = (metrics.watts > 0) ? pow((float)metrics.watts / 2.8, 1.0/3.0) : 0;
        metrics.totalDistance += (s * dt);
        metrics.paceSeconds = (s > 0) ? (uint16_t)(500.0 / s) : 0;
        metrics.cumulativeStrokes++;
    }
    metrics.lastStrokeTime = now;
}

void Physics::taskLoop(void * pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    bool isPulling = false;
    float accumulatedWorkJoules = 0.0;
    float currentPeakForce = 0.0;
    float previousCablePosition = 0.0;
    
    // Variabili per fusione sensori (Risolve encoder monotoni/ruote libere)
    int32_t lastExtTicks = 0;
    int32_t virtualTicks = 0;
    
    static int prescaler = 0;

    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1)); // Ciclo a 1000 Hz garantiti

        float fKg = loadCell.getKg();
        TelemetryRadio::checkTimeout();

        float currentCablePosition = 0.0;

        if (telemetry.active) {
            int32_t currentExtTicks = telemetry.encoderTicks;
            int32_t deltaTicks = abs(currentExtTicks - lastExtTicks);
            lastExtTicks = currentExtTicks;

            // Determina la fase della pagaiata usando la Forza come proxy della direzione
            if (fKg > config.pullThresh && !isPulling) {
                isPulling = true; 
                accumulatedWorkJoules = 0.0; 
                currentPeakForce = 0.0; 
            } 
            else if (fKg < config.relThresh && isPulling) {
                isPulling = false;
                processStrokeComplete(accumulatedWorkJoules, currentPeakForce);
            }

            // Applica i tick alla distanza virtuale a seconda della direzione (Tirata = +, Ritorno = -)
            if (isPulling) {
                virtualTicks += deltaTicks;
            } else {
                virtualTicks -= deltaTicks;
                if (virtualTicks < 0) virtualTicks = 0; // Azzera se il ritorno eccede
            }

            float metersPerTick = (config.pullCirc / 1000.0) / config.encPPR;
            currentCablePosition = (float)virtualTicks * metersPerTick; 
            float velocityMs = (currentCablePosition - previousCablePosition) / 0.001; 
            previousCablePosition = currentCablePosition;

            if (isPulling) {
                if (fKg > currentPeakForce) currentPeakForce = fKg;
                if (velocityMs > 0) accumulatedWorkJoules += (fKg * 9.81) * velocityMs * 0.001; 
            }
        } 
        else {
            if (fKg > config.pullThresh) {
                if (!isPulling) { isPulling = true; currentPeakForce = 0.0; }
                if (fKg > currentPeakForce) currentPeakForce = fKg;
            } 
            else if (fKg < config.relThresh && isPulling) {
                isPulling = false;
                float strokeLengthMeters = config.uHeight * 0.007;
                float workJoules = (currentPeakForce * 0.6 * 9.81) * strokeLengthMeters; 
                processStrokeComplete(workJoules, currentPeakForce);
            }
        }

        // --- INVIO RETE A 1000 Hz, REFRESH 20 Hz ---
        if (++prescaler >= 1) { 
            prescaler = 0;
            activeWriteBuffer[bufferIdx++] = fKg;
            activeWriteBuffer[bufferIdx++] = currentCablePosition;
            activeWriteBuffer[bufferIdx++] = telemetry.seatPositionMeters;
            
            if (bufferIdx >= 150) { 
                if (readyReadBuffer == nullptr) { 
                    readyReadBuffer = activeWriteBuffer;
                    activeWriteBuffer = (activeWriteBuffer == bufferA) ? bufferB : bufferA;
                }
                bufferIdx = 0;
            }
        }
    }
}