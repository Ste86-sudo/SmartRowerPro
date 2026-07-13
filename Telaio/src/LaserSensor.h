#pragma once

// Sensore ToF VL53L0X per la posizione del carrello (task dedicato su core 0)
class LaserSensor {
public:
    void begin();
};

extern LaserSensor laserSensor;
