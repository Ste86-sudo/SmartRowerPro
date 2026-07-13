#include "WebUIStandalone.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "CalibStore.h"
#include "PhysicsLite.h"
#include "LoadCell.h"
#include "WebUI_HTML.h"

WebUIStandalone webUIStandalone;
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// CFG: tara, scala, uHeight, uWeight, pullThresh, relThresh, encPPR, pullCirc,
//      laserOffset, wifiSSID, wifiPass, uFtp | macAddress    (formato invariato)
// NB: il campo SSID resta "RP_Handle" anche se il SoftAP si chiama RP_AP —
//     la UI lo usa come marker per riconoscere la modalità standalone.
static const char* buildCfgMsg() {
    static char buf[256];
    snprintf(buf, sizeof(buf), "CFG:%d,%.4f,%.1f,%.1f,%.1f,%.1f,1.0,1.0,0.0,RP_Handle,password,150.0|%s",
        calibStore.getTare(), calibStore.getScale(), calibStore.getUHeight(),
        calibStore.getUWeight(), calibStore.getPullThresh(), calibStore.getRelThresh(),
        WiFi.softAPmacAddress().c_str());
    return buf;
}

static void handleCfgCommand(AsyncWebSocketClient* c, uint8_t* d, size_t l) {
    char body[256];
    size_t n = l - 4;
    if (n >= sizeof(body)) n = sizeof(body) - 1;
    memcpy(body, d + 4, n);
    body[n] = 0;

    char* p = body;
    char* sTara = strsep(&p, ",");
    char* sScala = strsep(&p, ",");
    char* sUh = strsep(&p, ",");
    char* sUw = strsep(&p, ",");
    char* sUp = strsep(&p, ",");
    char* sUr = strsep(&p, ",");

    if (sTara && sScala) {
        calibStore.saveCalibration(atoi(sTara), atof(sScala));
    }
    if (sUp && sUr) {
        calibStore.saveThresholds(atof(sUp), atof(sUr));
    }
    if (sUh && sUw) {
        calibStore.saveProfile(atof(sUh), atof(sUw));
    }

    ws.textAll(buildCfgMsg());
}

static void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
    if (t != WS_EVT_DATA) return;

    AwsFrameInfo *info = (AwsFrameInfo*)a;
    if (!(info->final && info->index == 0 && info->len == l && info->opcode == WS_TEXT)) return;

    String cmd((char*)d, l);
    cmd.trim();

    if (cmd == "GET_CFG") {
        c->text(buildCfgMsg());

    } else if (cmd == "tare") {
        calibStore.saveCalibration(loadCell.getLastRaw(), calibStore.getScale());
        ws.textAll(buildCfgMsg());

    } else if (cmd.startsWith("calib:")) {
        float ref = cmd.substring(6).toFloat();
        if (ref > 0.01f) {
            float delta = (float)loadCell.getLastRaw() - (float)calibStore.getTare();
            float newScale = delta / ref;
            if (fabsf(newScale) > 0.001f) {
                calibStore.saveCalibration(calibStore.getTare(), newScale);
                ws.textAll(buildCfgMsg());
            }
        }

    } else if (cmd.startsWith("mech:")) {
        // formato: mech:<pullThresh>|<relThresh> — la UI usa questo comando per
        // aggiornare le soglie (in 1.0 era ignorato: parametri non aggiornabili)
        int splitIdx = cmd.indexOf('|');
        if (splitIdx > 0) {
            float p = cmd.substring(5, splitIdx).toFloat();
            float r = cmd.substring(splitIdx + 1).toFloat();
            calibStore.saveThresholds(p, r);
            ws.textAll(buildCfgMsg());
        }

    } else if (cmd == "REBOOT") {
        delay(500);
        ESP.restart();

    } else if (cmd == "GET_PBS") {
        // La maniglia non persiste i PB: risposta vuota compatibile con la UI
        c->text("PBS:100=0,200=0,500=0,1000=0,2000=0,5000=0,6000=0,10000=0,21097=0,42195=0");

    } else if (cmd.startsWith("CFG:")) {
        handleCfgCommand(c, d, l);
    }
}

void WebUIStandalone::begin() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
        AsyncWebServerResponse *resp = r->beginResponse(200, "text/html", index_html_gz, sizeof(index_html_gz));
        resp->addHeader("Content-Encoding", "gzip");
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        r->send(resp);
    });

    // OTA via browser (in 1.0 mancava: la maniglia non era aggiornabile via WiFi)
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

void WebUIStandalone::handleClients() {
    ws.cleanupClients(2);
}

void WebUIStandalone::sendTelemetryBuffer(float* readyBuffer) {
    if (ws.count() == 0 || !ws.availableForWriteAll()) return;
    if (readyBuffer) {
        ws.binaryAll((uint8_t*)readyBuffer, 39 * sizeof(float));
    }
}

void WebUIStandalone::sendMetrics() {
    if (ws.count() == 0 || !ws.availableForWriteAll()) return;

    // Campi 14 (dropped=0), 15 (fault=0) e 16 (colpi) per parità col telaio 2.0
    char p[220];
    snprintf(p, sizeof(p), "METRICS:%d|%d|%d|%d|%.1f|%s|%d|%u|%d|%d|%.2f|%.2f|%d|%.3f|0|0|%u",
             physicsLite.getWatts(), (int)physicsLite.getTotalDistance(), physicsLite.getSpm(),
             physicsLite.getPaceSeconds(), physicsLite.getTotalKcal(),
             (physicsLite.getIsPulling() ? "1" : "0"),
             (int)loadCell.getLastRaw(), 0u, 0, 0, physicsLite.getCurrentForce(), 0.0f,
             physicsLite.getIsPulling() ? 1 : 0, 0.0f, (unsigned)physicsLite.getStrokes());
    ws.textAll(p);
}
