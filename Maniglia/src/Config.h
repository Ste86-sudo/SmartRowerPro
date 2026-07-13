#pragma once

// --- PINOUT PIGGYBACK ADS1220 ---
#define ADS1220_DRDY_PIN  8
#define SPI_MISO          9
#define SPI_MOSI          10
#define SPI_SCK           11
#define ADS1220_CS_PIN    12

// --- PULSANTE MODALITÀ ---
#define BTN_STANDALONE_PIN 4   // INPUT_PULLUP: premuto al boot -> forza standalone

// RP_CHANNEL è definito in RowerProtocol.h (condiviso col telaio)
