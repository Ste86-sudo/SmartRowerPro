#include "LaserSensor.h"
#include "Config.h"
#include "SharedData.h"
#include "ConfigManager.h"
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

LaserSensor laserSensor;
static Adafruit_VL53L0X lox = Adafruit_VL53L0X();
static bool loxReady = false;

static void laserTask(void* pvParameters) {
    for (;;) {
        if (loxReady) {
            VL53L0X_RangingMeasurementData_t measure;
            lox.rangingTest(&measure, false);

            if (measure.RangeStatus != 4) {   // 4 = out of range
                float distanceMeters = (measure.RangeMilliMeter + config.laserOffset) / 1000.0f;
                if (distanceMeters < 0.0f) distanceMeters = 0.0f;

                portENTER_CRITICAL(&telemetryMux);
                telemetry.seatPositionMeters = distanceMeters;
                portEXIT_CRITICAL(&telemetryMux);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));   // ~33 Hz
    }
}

void LaserSensor::begin() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setTimeOut(150);
    Serial.println("[LASER] Inizializzazione VL53L0X...");

    if (!lox.begin(0x29, &Wire)) {
        Serial.println("[LASER] ERRORE: VL53L0X non trovato.");
        loxReady = false;
    } else {
        Serial.println("[LASER] VL53L0X trovato e pronto!");
        loxReady = true;
    }

    xTaskCreatePinnedToCore(laserTask, "LaserTask", 4096, NULL, 1, NULL, 0);
}
