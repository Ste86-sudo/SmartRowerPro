#pragma once

// --- PINOUT PIGGYBACK ADS1220 ---
#define ADS1220_DRDY_PIN  8
#define SPI_MISO          9   
#define SPI_MOSI          10  
#define SPI_SCK           11  
#define ADS1220_CS_PIN    12  

// --- PULSANTE MODALITÀ ---
#define BTN_STANDALONE_PIN 4   // INPUT_PULLUP: premuto al boot → standalone (brief Fase 4)

// --- RETE WIFI ---
#define RP_CHANNEL 6           // DEVE coincidere col telaio
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;