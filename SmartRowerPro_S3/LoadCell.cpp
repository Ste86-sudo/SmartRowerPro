#include "LoadCell.h"
#include "SharedData.h"
#include "ConfigManager.h"
#include <SPI.h>

LoadCellManager loadCell;

LoadCellManager::LoadCellManager() : ads(ADS1220_CS_PIN, ADS1220_DRDY_PIN) {}

void LoadCellManager::begin() {
    // 1. Configura i pin manualmente come GPIO prima
    pinMode(ADS1220_CS_PIN, OUTPUT);
    digitalWrite(ADS1220_CS_PIN, HIGH);
    
    // Usa INPUT_PULLUP nel caso in cui il modulo non abbia la resistenza di pull-up su DRDY
    pinMode(ADS1220_DRDY_PIN, INPUT_PULLUP);
    
    // 2. Svincola il CS dall'hardware SPI passando -1
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);
    
    // Assicuriamoci che l'SPI sia inizializzato correttamente per le comunicazioni ADS
    delay(10);
    
    // 3. Ora la libreria può comunicare e impostare i registri
    ads.init();
    ads.setCompareChannels(ADS1220_MUX_1_2);        
    ads.setVRefSource(ADS1220_VREF_AVDD_AVSS);      
    ads.setGain(ADS1220_GAIN_128);  
    
    // --- UPGRADE HARDWARE A 1000 Hz ---
    ads.setDataRate(ADS1220_DR_LVL_6); // Livello 6 = 1000 SPS 
    
    ads.setConversionMode(ADS1220_CONTINUOUS);
    ads.start();
}

int32_t LoadCellManager::getRaw() { 
    // Controllo non bloccante per evitare di congelare il Core 1
    if (digitalRead(ADS1220_DRDY_PIN) == LOW) {
        return ads.getRawData(); 
    }
    // Ritorna l'ultimo valore letto in caso il dato non sia pronto
    return metrics.rawAdc;
}

float LoadCellManager::getKg() {
    int32_t raw = getRaw();
    metrics.rawAdc = raw;
    return fabs((float)raw - config.tara) / config.scala;
}