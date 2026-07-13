#include "Physics.h"
#include <Arduino.h>
#include "SharedData.h"
#include "ConfigManager.h"
#include "Telemetry.h"
#include "Encoder.h"
#include "StrokeEngine.h"
#include "StrokeMath.h"
#include "GhostPhysics.h"

#define DEBUG_LOGS 0

// Periodo del loop fisico. I dati forza arrivano a 100 Hz dalla maniglia e il
// campionamento per il grafico è ogni 10 ms: 200 Hz bastano (era 1000 Hz).
static constexpr TickType_t kLoopPeriodTicks = pdMS_TO_TICKS(5);

// Matematica colpo condivisa (modalità encoder) e motore fallback (senza encoder)
static StrokeMath strokeMath;
static StrokeEngine engine;

// Ping-pong buffer per il grafico: Physics scrive, NetTask legge e libera
static float bufferA[TELEMETRY_BUFFER_SIZE];
static float bufferB[TELEMETRY_BUFFER_SIZE];
static float* activeWriteBuffer = bufferA;
static std::atomic<float*> readyReadBuffer{nullptr};
static int bufferIdx = 0;

float* Physics::getReadyBuffer() { return readyReadBuffer.load(std::memory_order_acquire); }
void Physics::clearReadyBuffer() { readyReadBuffer.store(nullptr, std::memory_order_release); }

void Physics::begin() {
    ghost.buildTemplate();
    ghost.updateCurves(20.0f, config.ghostDR, config.ghostLdrive, config.ghostB2, config.ghostMeff);
    strokeMath.reset();
    engine.begin();
    xTaskCreatePinnedToCore(taskLoop, "PhysicsTask", 8192, NULL, 2, NULL, 1);
}

// Colpo completato in modalità encoder: workJoules = integrale forza*spostamento
void Physics::processStrokeComplete(float workJoules) {
    unsigned long now = millis();

    StrokeMetrics m;
    bool valid = false;

    if (metrics.lastStrokeTime > 0) {
        float dt = (now - metrics.lastStrokeTime) / 1000.0f;
        if (dt < RowerModel::kMinStrokeSec) {
            return;   // debounce: scarta il colpo
        }
        strokeMath.compute(workJoules, dt, m);
        valid = true;

        ghost.updateCurves(m.spm, config.ghostDR, config.ghostLdrive, config.ghostB2, config.ghostMeff);
    }

    portENTER_CRITICAL(&telemetryMux);
    if (metrics.cumulativeStrokes == 0) {
        metrics.workoutStartMs = now;
    }
    if (valid) {
        metrics.spm = m.spm;
        metrics.watts = m.watts;
        metrics.paceSeconds = m.paceSeconds;
        metrics.totalDistance += m.dDist;
        metrics.totalKcal += m.dKcal;
    }
    metrics.cumulativeStrokes++;
    metrics.lastStrokeTime = now;
    portEXIT_CRITICAL(&telemetryMux);
}

void Physics::taskLoop(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    float accumulatedWorkJoules = 0.0f;
    float currentPeakForce = 0.0f;
    float previousCablePosition = 0.0f;
    bool isPulling = false;

    int64_t lastExtTicks = 0;
    int64_t virtualTicks = 0;
    // Direzione encoder appresa durante la tirata (il cavo esce => segno positivo):
    // permette di usare il delta CON segno, così il cavo "torna indietro" appena
    // il tamburo si riavvolge, non solo quando la forza scende sotto relThresh.
    int8_t encDir = 1;
    int64_t driveDirAccum = 0;
    bool prevActive = false;
    unsigned long zeroTimer = 0;
    unsigned long lastBufferTime = 0;
    // Picco vero del colpo in modalità fallback: massimo dei peakCenti ricevuti
    // durante l'intera tirata (il singolo pacchetto copre solo 10 ms)
    float fallbackPeak = 0.0f;

    // Auto-diagnosi encoder: tirata valida (>600 ms) con tamburo praticamente
    // fermo (<20 tick) → strike; 2 strike consecutivi → fault e passaggio
    // automatico alla stima da picco. Rientro se l'encoder torna a muoversi.
    int64_t driveAbsTicks = 0;
    unsigned long driveStartMs = 0;
    uint8_t faultStrikes = 0;
    int64_t fbAbsTicks = 0;

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, kLoopPeriodTicks);

        bool isNewData = false;
        if (millis() - lastBufferTime >= 10) {
            isNewData = true;
            lastBufferTime = millis();
        }

        TelemetryRadio::checkTimeout();
        portENTER_CRITICAL(&telemetryMux);
        float fKg = telemetry.currentForce;
        bool isActive = telemetry.active;
        portEXIT_CRITICAL(&telemetryMux);

        float currentCablePosition = 0.0f;
        float metersPerTick = config.getMetersPerTick();

        int64_t currentExtTicks = encoderManager.getCount();

        // Sincronizza i ticks al (re)connect per evitare spike di velocità
        if (isActive && !prevActive) {
            lastExtTicks = currentExtTicks;
        }
        // Reset stato pulling al dropout della maniglia: evita joule fantasma
        if (!isActive && prevActive && isPulling) {
            isPulling = false;
            accumulatedWorkJoules = 0.0f;
            currentPeakForce = 0.0f;
        }
        prevActive = isActive;

        if (isActive && config.encoderEnabled && !telemetry.encoderFault) {
            // --- MODALITÀ ENCODER: lavoro reale integrato forza * spostamento ---
            int64_t signedDelta = currentExtTicks - lastExtTicks;
            lastExtTicks = currentExtTicks;

            if (fKg > config.pullThresh && !isPulling) {
                isPulling = true;
                accumulatedWorkJoules = 0.0f;
                currentPeakForce = 0.0f;
                // Reset al catch: ignora eventuali passi persi nel ritorno
                virtualTicks = 0;
                driveDirAccum = 0;
                driveAbsTicks = 0;
                driveStartMs = millis();
            }
            else if (fKg < config.relThresh && isPulling) {
                isPulling = false;
                // Durante la tirata il cavo esce: il segno netto del movimento
                // definisce la direzione "positiva" dell'encoder per i cicli successivi
                if (driveDirAccum != 0) encDir = (driveDirAccum > 0) ? 1 : -1;

                // Auto-diagnosi: tirata lunga ma encoder fermo?
                if (millis() - driveStartMs > 600 && driveAbsTicks < 20) {
                    if (++faultStrikes >= 2) {
                        portENTER_CRITICAL(&telemetryMux);
                        telemetry.encoderFault = true;
                        portEXIT_CRITICAL(&telemetryMux);
                        engine.begin();       // il prossimo colpo parte in fallback pulito
                        fbAbsTicks = 0;
                        Serial.println("[PHYSICS] ENCODER FAULT: passo alla stima da picco forza");
                    }
                } else {
                    faultStrikes = 0;
                }

                processStrokeComplete(accumulatedWorkJoules);
            }

            if (isPulling) {
                driveDirAccum += signedDelta;
                driveAbsTicks += llabs(signedDelta);
            }

            // Delta con segno: la posizione segue il tamburo in entrambe le fasi,
            // quindi il ritorno inizia appena il cavo si riavvolge (fix 1.0:
            // tornava indietro solo dopo che la forza scendeva sotto relThresh)
            virtualTicks += encDir * signedDelta;
            if (virtualTicks < 0) virtualTicks = 0;

            currentCablePosition = (float)virtualTicks * metersPerTick;
            float deltaPos = currentCablePosition - previousCablePosition;
            previousCablePosition = currentCablePosition;

            // Auto-azzeramento se fermo da 2 s con forza nulla
            if (fabsf(deltaPos) < 0.000005f && fKg < 1.0f) {
                if (zeroTimer == 0) zeroTimer = millis();
                else if (millis() - zeroTimer > 2000) {
                    virtualTicks = 0;
                    currentCablePosition = 0.0f;
                }
            } else {
                zeroTimer = 0;
            }

            if (isPulling) {
                if (fKg > currentPeakForce) currentPeakForce = fKg;
                if (deltaPos > 0.0f) accumulatedWorkJoules += (fKg * RowerModel::kGravity) * deltaPos;
            }
        }
        else {
            // --- FALLBACK: encoder disattivato (toggle) o maniglia assente ---
            // Watt/distanza/kcal stimati dal picco di forza del colpo.
            float strokeLengthMeters = config.uHeight * 0.007f;
            portENTER_CRITICAL(&telemetryMux);
            float remotePeak = telemetry.remotePeakForce;
            portEXIT_CRITICAL(&telemetryMux);

            // Tieni sincronizzati i ticks: evita uno spike di posizione se
            // l'encoder viene riattivato dal toggle a runtime. Il delta serve
            // anche al rientro automatico dal fault.
            int64_t fbDelta = currentExtTicks - lastExtTicks;
            lastExtTicks = currentExtTicks;

            bool wasPulling = engine.isPulling();
            if (wasPulling) fbAbsTicks += llabs(fbDelta);

            StrokeMetrics res;
            bool strokeDone = engine.update(fKg, config.pullThresh, config.relThresh,
                                            strokeLengthMeters, millis(), res,
                                            fallbackPeak > 0.0f ? fallbackPeak : -1.0f);

            // Accumula il picco vero del colpo: peakCenti copre solo la finestra
            // TX di 10 ms, quindi va massimizzato lungo tutta la tirata
            if (!wasPulling && engine.isPulling()) {
                fallbackPeak = remotePeak;                       // catch: nuovo colpo
            } else if (engine.isPulling() && remotePeak > fallbackPeak) {
                fallbackPeak = remotePeak;
            }

            if (strokeDone) {
                fallbackPeak = 0.0f;

                // Rientro dal fault: l'encoder si è mosso per bene durante la tirata
                if (telemetry.encoderFault && fbAbsTicks > 100) {
                    portENTER_CRITICAL(&telemetryMux);
                    telemetry.encoderFault = false;
                    portEXIT_CRITICAL(&telemetryMux);
                    faultStrikes = 0;
                    Serial.println("[PHYSICS] Encoder di nuovo attivo: riabilito le metriche reali");
                }
                fbAbsTicks = 0;

                portENTER_CRITICAL(&telemetryMux);
                if (metrics.cumulativeStrokes == 0) {
                    metrics.workoutStartMs = millis();
                }
                metrics.spm = res.spm;
                metrics.watts = res.watts;
                metrics.paceSeconds = res.paceSeconds;
                metrics.totalDistance += res.dDist;
                metrics.totalKcal += res.dKcal;
                metrics.cumulativeStrokes++;
                metrics.lastStrokeTime = millis();
                portEXIT_CRITICAL(&telemetryMux);
            }
            isPulling = engine.isPulling();
        }

        // Campiona la terzina per il grafico ogni 10 ms
        if (isNewData) {
            activeWriteBuffer[bufferIdx++] = fKg;
            activeWriteBuffer[bufferIdx++] = currentCablePosition;
            // seatPositionMeters è aggiornato dal task LaserSensor sul core 0
            activeWriteBuffer[bufferIdx++] = telemetry.seatPositionMeters;

            if (bufferIdx >= (int)TELEMETRY_BUFFER_SIZE) {
                if (readyReadBuffer.load(std::memory_order_acquire) == nullptr) {
                    readyReadBuffer.store(activeWriteBuffer, std::memory_order_release);
                    activeWriteBuffer = (activeWriteBuffer == bufferA) ? bufferB : bufferA;
                }
                bufferIdx = 0;
            }
        }

        // Pubblica lo stato istantaneo
        portENTER_CRITICAL(&telemetryMux);
        telemetry.currentForce = fKg;
        telemetry.isPulling = isPulling;
        telemetry.currentCablePosition = currentCablePosition;
        telemetry.encoderTicks = (int32_t)currentExtTicks;
        portEXIT_CRITICAL(&telemetryMux);

#if DEBUG_LOGS
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 500) {
            lastDebug = millis();
            Serial.printf("[DEBUG] Enc: %d, ToF: %.2fm, HR: %d, fKg: %.2f, Pull: %d, Pos: %.3f\n",
                telemetry.encoderTicks, telemetry.seatPositionMeters, telemetry.heartRate,
                fKg, isPulling, currentCablePosition);
        }
#endif
    }
}
