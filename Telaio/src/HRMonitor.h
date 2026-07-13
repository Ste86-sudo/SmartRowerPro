#pragma once

// Client BLE per fascia cardio standard (servizio 0x180D)
class HRMonitor {
public:
    void begin();
    void loop();   // chiamare periodicamente dal NetTask (gestisce scan/reconnect)
};

extern HRMonitor hrMonitor;
