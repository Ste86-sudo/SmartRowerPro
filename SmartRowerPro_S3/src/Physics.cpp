#include "Physics.h"
#include <Arduino.h>
#include "SharedData.h"
#include "ConfigManager.h"
#include "LoadCell.h"
#include "Telemetry.h"

#define DEBUG_LOGS 0 // DIAG temporaneo: disabilitato post-test stabilità

// Ping-Pong buffer per il grafico
static float bufferA[TELEMETRY_BUFFER_SIZE];
static float bufferB[TELEMETRY_BUFFER_SIZE];
static float* activeWriteBuffer = bufferA;
static std::atomic<float*> readyReadBuffer{nullptr};
static int bufferIdx = 0;

float* Physics::getReadyBuffer() { return readyReadBuffer.load(std::memory_order_acquire); }
void Physics::clearReadyBuffer() { readyReadBuffer.store(nullptr, std::memory_order_release); }

void Physics::begin() {
    xTaskCreatePinnedToCore(taskLoop, "PhysicsTask", 8192, NULL, 2, NULL, 1);
}

void Physics::processStrokeComplete(float workJoules, float peakForce) {
    unsigned long now = millis();
    
    // Variabili d'appoggio per calcoli fuori dal lock
    uint16_t newSpm = 0, newWatts = 0, newPace = 0;
    float dKcal = 0.0f, dDist = 0.0f;
    unsigned long workoutStartUpdate = 0;

    if (metrics.lastStrokeTime > 0) {
        float dt = (now - metrics.lastStrokeTime) / 1000.0f;
        if (dt < 0.001f) { 
            metrics.lastStrokeTime = now;
            return; 
        }

        newSpm = (uint16_t)(60.0f / dt);
        newWatts = (uint16_t)(workJoules / dt);
        dKcal = ((workJoules / 4184.0f) * 4.0f);
        
        float s = (newWatts > 0) ? powf((float)newWatts / 2.8f, 1.0f/3.0f) : 0.0f;
        dDist = (s * dt);
        newPace = (s > 0.0f) ? (uint16_t)(500.0f / s) : 0;
    }

    // Pubblicazione atomica di tutte le metriche
    if (metrics.lastStrokeTime > 0) {
        if (metrics.cumulativeStrokes == 0) {
            metrics.workoutStartMs = now;
        }
        metrics.spm = newSpm;
        metrics.watts = newWatts;
        metrics.totalKcal += dKcal;
        metrics.totalDistance += dDist;
        metrics.paceSeconds = newPace;
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
    
    int32_t lastExtTicks = 0;
    int32_t virtualTicks = 0;
    bool prevActive = false;
    unsigned long zeroTimer = 0;

    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1)); // Ciclo a 1000 Hz garantiti

        bool isNewData = false;
        float fKg = loadCell.getKg(isNewData);
        TelemetryRadio::checkTimeout();

        float currentCablePosition = 0.0f;
        float metersPerTick = config.getMetersPerTick();
        
        bool isActive = telemetry.active;
        int32_t currentExtTicks = telemetry.encoderTicks;

        // Sincronizza ticks al (re)connect per evitare spike di velocità
        if (isActive && !prevActive) {
            lastExtTicks = currentExtTicks;
        }
        // Reset stato pulling al dropout C3 per evitare accumulo joule fantasma
        if (!isActive && prevActive && isPulling) {
            isPulling = false;
            accumulatedWorkJoules = 0.0f;
            currentPeakForce = 0.0f;
        }
        prevActive = isActive;

        if (isActive) {
            int32_t deltaTicks = abs(currentExtTicks - lastExtTicks);
            lastExtTicks = currentExtTicks;

            if (fKg > config.pullThresh && !isPulling) {
                isPulling = true;
                accumulatedWorkJoules = 0.0f;
                currentPeakForce = 0.0f;
                // Reset a zero ad ogni colpo (catch) per ignorare passi persi nel ritorno
                virtualTicks = 0;
            }
            else if (fKg < config.relThresh && isPulling) {
                isPulling = false;
                processStrokeComplete(accumulatedWorkJoules, currentPeakForce);
            }

            if (isPulling) {
                virtualTicks += deltaTicks;
            } else {
                virtualTicks -= deltaTicks;
                if (virtualTicks < 0) virtualTicks = 0;
            }

            currentCablePosition = (float)virtualTicks * metersPerTick;
            float deltaPos = currentCablePosition - previousCablePosition;
            float velocityMs = deltaPos * 1000.0f;
            previousCablePosition = currentCablePosition;

            // Auto-reset a zero se fermo da 2 secondi con forza nulla
            if (fabsf(velocityMs) < 0.001f && fKg < 1.0f) {
                if (zeroTimer == 0) zeroTimer = millis();
                else if (millis() - zeroTimer > 2000) {
                    virtualTicks = 0;
                    currentCablePosition = 0.0f; // Azzera anche la posizione corrente
                }
            } else {
                zeroTimer = 0;
            }

            if (isPulling) {
                if (fKg > currentPeakForce) currentPeakForce = fKg;
                if (deltaPos > 0.0f) accumulatedWorkJoules += (fKg * 9.81f) * deltaPos;
            }
        }
        else {
            if (fKg > config.pullThresh) {
                if (!isPulling) { isPulling = true; currentPeakForce = 0.0f; }
                if (fKg > currentPeakForce) currentPeakForce = fKg;
            }
            else if (fKg < config.relThresh && isPulling) {
                isPulling = false;
                float strokeLengthMeters = config.uHeight * 0.007f;
                float workJoules = (currentPeakForce * 0.6f * 9.81f) * strokeLengthMeters;
                processStrokeComplete(workJoules, currentPeakForce);
            }
        }

        // --- INVIO RETE: popola il buffer SOLO se abbiamo un nuovo dato dalla cella di carico ---
        if (isNewData) { 
            activeWriteBuffer[bufferIdx++] = fKg;
            activeWriteBuffer[bufferIdx++] = currentCablePosition;
            // NOTA: seatPositionMeters e' aggiornato in un altro task, ma su ESP32 (arch a 32-bit) 
            // le letture/scritture allineate a 32-bit (come un float) sono intrinsecamente atomiche a livello hardware.
            // Il tearing del singolo float non puo' avvenire.
            activeWriteBuffer[bufferIdx++] = telemetry.seatPositionMeters;
            
            if (bufferIdx >= TELEMETRY_BUFFER_SIZE) { 
                if (readyReadBuffer.load(std::memory_order_acquire) == nullptr) { 
                    readyReadBuffer.store(activeWriteBuffer, std::memory_order_release);
                    activeWriteBuffer = (activeWriteBuffer == bufferA) ? bufferB : bufferA;
                }
                bufferIdx = 0;
            }
        }
        
        // Scrittura dei dati in global space (proteggiamo la coerenza della struct con il lock)
        telemetry.currentForce = fKg;
        telemetry.isPulling = isPulling;
        telemetry.currentCablePosition = currentCablePosition;

#if DEBUG_LOGS
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 500) {
            lastDebug = millis();
            Serial.printf("[DEBUG] Raw Enc: %d, ToF: %.2fm, HR: %d, fKg: %.2f, Pulling: %d, Pos: %.3f (vTicks: %d, mPt: %.6f)\n", 
                telemetry.encoderTicks, telemetry.seatPositionMeters, telemetry.heartRate, fKg, isPulling, currentCablePosition, virtualTicks, metersPerTick);
        }
#endif
    }
}