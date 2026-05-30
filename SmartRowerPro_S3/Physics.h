#pragma once

class Physics {
public:
    static void begin();
    static volatile float* getReadyBuffer();
    static void clearReadyBuffer();

private:
    static void taskLoop(void * pvParameters);
    static void processStrokeComplete(float workJoules, float peakForce);
};