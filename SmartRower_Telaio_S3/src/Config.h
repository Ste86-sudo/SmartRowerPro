#pragma once

// --- PINOUT ENCODER ---
#define ENC_PIN_A 4
#define ENC_PIN_B 5

// --- RETE WIFI ---
#define RP_CHANNEL 6          // Canale fisso ESP-NOW/AP (brief §5.4) — DEVE coincidere con la maniglia
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// --- I2C LASER ---
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9