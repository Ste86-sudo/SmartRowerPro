#include "WebUI.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "SharedData.h"
#include "ConfigManager.h"
#include "Physics.h"
#include "WebUI_HTML.h"
#include "Telemetry.h"

WebUI webApp;

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// Costruisce la stringa CFG da inviare al client
static String buildCfgMsg() {
    char buf[256];
    snprintf(buf, sizeof(buf), "CFG:%d,%.4f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%s,%s,%.1f|%s",
        config.tara, config.scala, config.uHeight, config.uWeight,
        config.pullThresh, config.relThresh, config.encPPR,
        config.pullCirc, config.laserOffset,
        config.wifiSSID.c_str(), config.wifiPass.c_str(), config.uFtp,
        WiFi.softAPmacAddress().c_str());
    return String(buf);
}

void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
    if (t == WS_EVT_CONNECT) {
        // Non inviare dati qui (rischio close 1006): il client richiede la CFG via "GET_CFG".
        return;
    }
    if (t != WS_EVT_DATA) return;

    AwsFrameInfo *info = (AwsFrameInfo*)a;
    if (!(info->final && info->index == 0 && info->len == l && info->opcode == WS_TEXT)) return;

    d[l] = 0;
    String cmd = String((char*)d);
    cmd.trim();

    if (cmd == "GET_CFG") {
        c->text(buildCfgMsg());

    } else if (cmd == "tare") {
        TelemetryRadio::sendTare();
        // Nota: Il telaio non salva più la tara localmente. Invia il comando alla maniglia.
        ws.textAll(buildCfgMsg());

    } else if (cmd.startsWith("calib:")) {
        float ref = cmd.substring(6).toFloat();
        if (ref > 0.01f) {
            TelemetryRadio::sendCalib(ref);
            ws.textAll(buildCfgMsg());
        }

    } else if (cmd == "SCAN") {
        WiFi.scanNetworks(true, true);

    } else if (cmd == "REBOOT") {
        delay(500); ESP.restart();

    } else if (cmd == "GET_PBS") {
        char buf[256];
        snprintf(buf, sizeof(buf), "PBS:100=%u,200=%u,500=%u,1000=%u,2000=%u,5000=%u,6000=%u,10000=%u,21097=%u,42195=%u",
            config.getPB(100), config.getPB(200), config.getPB(500), config.getPB(1000), config.getPB(2000),
            config.getPB(5000), config.getPB(6000), config.getPB(10000), config.getPB(21097), config.getPB(42195));
        c->text(String(buf));

    } else if (cmd.startsWith("SAVE_PB:")) {
        int idx = cmd.indexOf(',');
        if (idx > 0) {
            uint16_t dist = cmd.substring(8, idx).toInt();
            uint32_t timeSec = cmd.substring(idx + 1).toInt();
            config.savePB(dist, timeSec);
        }

    } else if (cmd.startsWith("CFG:")) {
        char* body = (char*)d + 4;
        char* sTara = strsep(&body, ",");
        char* sScala = strsep(&body, ",");
        char* sUh = strsep(&body, ",");
        char* sUw = strsep(&body, ",");
        char* sUp = strsep(&body, ",");
        char* sUr = strsep(&body, ",");
        char* sEppr = strsep(&body, ",");
        char* sPcirc = strsep(&body, ",");
        char* sLoff = strsep(&body, ",");
        char* sSSID = strsep(&body, ",");
        char* sPass = strsep(&body, ",");
        char* sFtp = strsep(&body, ",");

        if (!sTara || !sEppr) return;
        
        config.saveTara(atoi(sTara));
        config.saveScale(atof(sScala));
        config.saveProfile(atof(sUh), atof(sUw), atof(sUp), atof(sUr));
        
        float fPcirc = (sPcirc && *sPcirc) ? atof(sPcirc) : config.pullCirc;
        float fLoff  = (sLoff && *sLoff) ? atof(sLoff) : config.laserOffset;
        config.saveMechanics(atof(sEppr), fPcirc, fLoff);

        if (sFtp && *sFtp) {
            config.uFtp = atof(sFtp);
            Preferences prefs;
            prefs.begin("rower", false);
            prefs.putFloat("uFtp", config.uFtp);
            prefs.end();
        }

        if (sSSID && *sSSID) {
            config.saveWiFi(String(sSSID), sPass ? String(sPass) : "");
        }
        
        c->text(buildCfgMsg());
        ws.textAll(buildCfgMsg());
    }
}

void WebUI::begin() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
        AsyncWebServerResponse *resp = r->beginResponse_P(200, "text/html", index_html_gz, sizeof(index_html_gz));
        resp->addHeader("Content-Encoding", "gzip");
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        r->send(resp);
    });

    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *r){
        r->send(200, "text/html",
            "<!DOCTYPE html><html><body style='background:#020617;color:#fff;text-align:center;'>"
            "<form method='POST' action='/update' enctype='multipart/form-data'>"
            "<input type='file' name='update'><input type='submit' value='FLASH'>"
            "</form></body></html>");
    });

    server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *r){
            bool ok = !Update.hasError();
            AsyncWebServerResponse *res = r->beginResponse(200, "text/plain", ok ? "OK" : "ERR");
            res->addHeader("Connection", "close");
            r->send(res);
            if (ok) { delay(500); ESP.restart(); }
        },
        [](AsyncWebServerRequest *r, String f, size_t i, uint8_t *d, size_t l, bool fin){
            if (!i) Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
            if (!Update.hasError()) Update.write(d, l);
            if (fin) Update.end(true);
        }
    );

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
}

void WebUI::handleClients() {
    // ESPAsyncWebServer è asincrono — cleanupClients libera memoria dei client disconnessi
    ws.cleanupClients(2);

    // Controlla se la scansione WiFi asincrona ha prodotto risultati
    int16_t n = WiFi.scanComplete();
    if (n >= 0) {
        String res;
        res.reserve(256);
        res = "WIFI_LIST:";
        for (int i = 0; i < n; ++i) {
            if (i > 0) res += ",";
            res += WiFi.SSID(i);
        }
        ws.textAll(res);
        WiFi.scanDelete();
    }
}

void WebUI::sendTelemetryBuffer(float* readyBuffer) {
    if (ws.count() == 0 || !ws.availableForWriteAll()) return;
    if (readyBuffer) {
        ws.binaryAll((uint8_t*)readyBuffer, TELEMETRY_BUFFER_SIZE * sizeof(float));
        // clearReadyBuffer() rimosso: la liberazione avviene nel loop() (vedi main.cpp)
    }
    syncPacketCounter++;
}

void WebUI::sendMetrics() {
    if (ws.count() == 0 || !ws.availableForWriteAll()) return;

    RowerMetrics localM;
    TelemetryData localT;
    uint32_t localSync;
    
    portENTER_CRITICAL(&telemetryMux);
    localM = metrics;
    localT = telemetry;
    localSync = syncPacketCounter;
    portEXIT_CRITICAL(&telemetryMux);

    char p[200];
    snprintf(p, sizeof(p), "METRICS:%d|%d|%d|%d|%.1f|%s|%d|%u|%d|%d|%.2f|%.2f|%d|%.3f",
             localM.watts, (int)localM.totalDistance, localM.spm, localM.paceSeconds,
             localM.totalKcal, (localT.active ? "1" : "0"),
             localM.rawAdc, localSync, localT.heartRate,
             localT.encoderTicks, localT.seatPositionMeters, localT.currentForce, localT.isPulling ? 1 : 0,
             localT.currentCablePosition);
    ws.textAll(p);
}
