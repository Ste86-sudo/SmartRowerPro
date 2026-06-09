#include "SharedData.h"
#include "Config.h"

const char* WIFI_SSID = "RP_AP";
const char* WIFI_PASS = "password"; 

RowerMetrics metrics = {0, 0, 0, 0.0f, 0, 0.0f, 0, 0, 0};
TelemetryData telemetry = {0, 0.0f, 0, 0, false};
volatile uint32_t syncPacketCounter = 0;
