#include "WebUI.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "SharedData.h"
#include "ConfigManager.h"
#include "Physics.h"
#include "WebUI_HTML.h"
#include "Telemetry.h"
#include "GhostPhysics.h"

WebUI webApp;

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// Buffer per il JSON del ghost (usato solo dal task async_tcp)
static char ghostJsonBuf[2560];

// Costruisce il messaggio CFG per il client.
// Formato 1.0 + quarta sezione "|<encoderEnabled>" (la UI vecchia la ignora)
static const char* buildCfgMsg() {
    static char buf[280];
    snprintf(buf, sizeof(buf), "CFG:%d,%.4f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%s,%s,%.1f|%s|%.2f,%.2f,%.2f,%.2f|%d",
        config.tara, config.scala, config.uHeight, config.uWeight,
        config.pullThresh, config.relThresh, config.encPPR,
        config.pullCirc, config.laserOffset,
        config.wifiSSID.c_str(), config.wifiPass.c_str(), config.uFtp,
        WiFi.softAPmacAddress().c_str(),
        config.ghostB2, config.ghostMeff, config.ghostLdrive, config.ghostDR,
        config.encoderEnabled ? 1 : 0);
    return buf;
}

static void handleCfgCommand(AsyncWebSocketClient* c, uint8_t* d, size_t l) {
    char body[256];
    size_t n = l - 4;
    if (n >= sizeof(body)) n = sizeof(body) - 1;
    memcpy(body, d + 4, n);
    body[n] = 0;

    char* pPtr = body;
    char* sTara = strsep(&pPtr, ",");
    char* sScala = strsep(&pPtr, ",");
    char* sUh = strsep(&pPtr, ",");
    char* sUw = strsep(&pPtr, ",");
    char* sUp = strsep(&pPtr, ",");
    char* sUr = strsep(&pPtr, ",");
    char* sEppr = strsep(&pPtr, ",");
    char* sPcirc = strsep(&pPtr, ",");
    char* sLoff = strsep(&pPtr, ",");
    char* sSSID = strsep(&pPtr, ",");
    char* sPass = strsep(&pPtr, ",");
    char* sFtp = strsep(&pPtr, ",");

    if (!sTara || !sEppr) return;

    config.saveTara(atoi(sTara));
    config.saveScale(atof(sScala));
    config.saveProfile(atof(sUh), atof(sUw), atof(sUp), atof(sUr));

    // Propaga tara/scala alla maniglia
    TelemetryRadio::sendSetTare(atoi(sTara));
    TelemetryRadio::sendSetScale(atof(sScala));

    float fPcirc = (sPcirc && *sPcirc) ? atof(sPcirc) : config.pullCirc;
    float fLoff  = (sLoff && *sLoff) ? atof(sLoff) : config.laserOffset;
    config.saveMechanics(atof(sEppr), fPcirc, fLoff);

    if (sFtp && *sFtp) {
        config.saveFtp(atof(sFtp));
    }
    if (sSSID && *sSSID) {
        config.saveWiFi(String(sSSID), sPass ? String(sPass) : "");
    }

    // textAll raggiunge anche il client che ha inviato la CFG
    ws.textAll(buildCfgMsg());
}

static void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
    if (t == WS_EVT_CONNECT) {
        // Non inviare dati qui (rischio close 1006): il client richiede la CFG via "GET_CFG"
        return;
    }
    if (t != WS_EVT_DATA) return;

    AwsFrameInfo *info = (AwsFrameInfo*)a;
    if (!(info->final && info->index == 0 && info->len == l && info->opcode == WS_TEXT)) return;

    String cmd((char*)d, l);
    cmd.trim();

    if (cmd == "GET_CFG") {
        c->text(buildCfgMsg());

    } else if (cmd == "tare") {
        // La tara viene eseguita e salvata sulla maniglia
        TelemetryRadio::sendTare();
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
        delay(500);
        ESP.restart();

    } else if (cmd == "GET_PBS") {
        char buf[256];
        snprintf(buf, sizeof(buf), "PBS:100=%u,200=%u,500=%u,1000=%u,2000=%u,5000=%u,6000=%u,10000=%u,21097=%u,42195=%u",
            config.getPB(100), config.getPB(200), config.getPB(500), config.getPB(1000), config.getPB(2000),
            config.getPB(5000), config.getPB(6000), config.getPB(10000), config.getPB(21097), config.getPB(42195));
        c->text(buf);

    } else if (cmd.startsWith("SAVE_PB:")) {
        int idx = cmd.indexOf(',');
        if (idx > 0) {
            uint16_t dist = cmd.substring(8, idx).toInt();
            uint32_t timeSec = cmd.substring(idx + 1).toInt();
            config.savePB(dist, timeSec);
        }

    } else if (cmd.startsWith("encmode:")) {
        // encmode:1 = metriche dall'encoder; encmode:0 = stima dal picco forza
        config.saveEncoderMode(cmd.substring(8).toInt() != 0);
        ws.textAll(buildCfgMsg());

    } else if (cmd.startsWith("mech:")) {
        // formato: mech:<pullThresh>|<relThresh>
        int splitIdx = cmd.indexOf('|');
        if (splitIdx > 0) {
            float p = cmd.substring(5, splitIdx).toFloat();
            float r = cmd.substring(splitIdx + 1).toFloat();
            config.saveThresholds(p, r);
            ws.textAll(buildCfgMsg());
        }

    } else if (cmd.startsWith("ghost:")) {
        // formato: ghost:<b2>|<meff>|<ldrive>|<dr>
        String body = cmd.substring(6);
        int i1 = body.indexOf('|');
        int i2 = body.indexOf('|', i1 + 1);
        int i3 = body.indexOf('|', i2 + 1);
        if (i1 > 0 && i2 > i1 && i3 > i2) {
            float b2 = body.substring(0, i1).toFloat();
            float me = body.substring(i1 + 1, i2).toFloat();
            float ld = body.substring(i2 + 1, i3).toFloat();
            float dr = body.substring(i3 + 1).toFloat();
            config.saveGhost(b2, me, ld, dr);
            ghost.updateCurves(20.0f, config.ghostDR, config.ghostLdrive, config.ghostB2, config.ghostMeff);
            ghost.getJSON(ghostJsonBuf, sizeof(ghostJsonBuf));
            ws.textAll(ghostJsonBuf);
        }

    } else if (cmd.startsWith("CFG:")) {
        handleCfgCommand(c, d, l);
    }
}

void WebUI::begin() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
        AsyncWebServerResponse *resp = r->beginResponse_P(200, "text/html", index_html_gz, sizeof(index_html_gz));
        resp->addHeader("Content-Encoding", "gzip");
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        r->send(resp);
    });

    server.on("/ghost", HTTP_GET, [](AsyncWebServerRequest *r){
        ghost.getJSON(ghostJsonBuf, sizeof(ghostJsonBuf));
        r->send(200, "application/json", ghostJsonBuf);
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
    // Libera la memoria dei client WebSocket disconnessi
    ws.cleanupClients(2);

    // Esito della scansione WiFi asincrona
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
        // la liberazione del buffer avviene nel NetTask (main.cpp)
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

    // Campi 15 (encoderFault) e 16 (colpi totali) aggiunti in coda:
    // le UI vecchie li ignorano
    char p[260];
    snprintf(p, sizeof(p), "METRICS:%d|%d|%d|%d|%.1f|%s|%d|%u|%d|%d|%.2f|%.2f|%d|%.3f|%u|%d|%u",
             localM.watts, (int)localM.totalDistance, localM.spm, localM.paceSeconds,
             localM.totalKcal, (localT.active ? "1" : "0"),
             localM.rawAdc, localSync, localT.heartRate,
             localT.encoderTicks, localT.seatPositionMeters, localT.currentForce, localT.isPulling ? 1 : 0,
             localT.currentCablePosition, localT.droppedPackets, localT.encoderFault ? 1 : 0,
             (unsigned)localM.cumulativeStrokes);
    ws.textAll(p);
}
