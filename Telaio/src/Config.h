#pragma once

// --- PINOUT ENCODER ---
#define ENC_PIN_A 4
#define ENC_PIN_B 5

// --- I2C LASER (VL53L0X) ---
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// --- RETE ---
// RP_CHANNEL è definito in RowerProtocol.h (condiviso con la maniglia)
extern const char* WIFI_SSID;   // SoftAP del telaio
extern const char* WIFI_PASS;
