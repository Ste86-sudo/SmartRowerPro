#include "WebUIStandalone.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "CalibStore.h"
#include "PhysicsLite.h"
#include "LoadCell.h"
#include "WebUI_HTML.h"

WebUIStandalone webUIStandalone;
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

static String buildCfgMsg() {
    char buf[256];
    // CFG: tara, scala, uHeight, uWeight, pullThresh, relThresh, encPPR, pullCirc, laserOffset, wifiSSID, wifiPass, uFtp, macAddress
    snprintf(buf, sizeof(buf), "CFG:%d,%.4f,%.1f,%.1f,%.1f,%.1f,1.0,1.0,0.0,RP_Handle,password,150.0|%s",
        calibStore.getTare(), calibStore.getScale(), calibStore.getUHeight(), calibStore.getUWeight(), calibStore.getPullThresh(), calibStore.getRelThresh(), WiFi.softAPmacAddress().c_str());
    return String(buf);
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
    } else if (cmd == "REBOOT") {
        delay(500); ESP.restart();
    } else if (cmd == "GET_PBS") {
        c->text("PBS:100=0,200=0,500=0,1000=0,2000=0,5000=0,6000=0,10000=0,21097=0,42195=0");
    } else if (cmd.startsWith("CFG:")) {
        char bodyCopy[256];
        size_t n = l - 4;
        if (n >= sizeof(bodyCopy)) n = sizeof(bodyCopy) - 1;
        memcpy(bodyCopy, d + 4, n);
        bodyCopy[n] = 0;

        char* p = bodyCopy;
        char* sTara = strsep(&p, ","); // tara
        char* sScala = strsep(&p, ","); // scala
        char* sUh = strsep(&p, ","); // uHeight
        char* sUw = strsep(&p, ","); // uWeight
        char* sUp = strsep(&p, ","); // pullThresh
        char* sUr = strsep(&p, ","); // relThresh

        if (sTara && sScala) {
            calibStore.saveCalibration(atoi(sTara), atof(sScala));
        }
        if (sUp && sUr) {
            calibStore.saveThresholds(atof(sUp), atof(sUr));
        }
        if (sUh && sUw) {
            calibStore.saveProfile(atof(sUh), atof(sUw));
        }
        
        c->text(buildCfgMsg());
        ws.textAll(buildCfgMsg());
    }
}

void WebUIStandalone::begin() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
        AsyncWebServerResponse *resp = r->beginResponse(200, "text/html", index_html_gz, sizeof(index_html_gz));
        resp->addHeader("Content-Encoding", "gzip");
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        r->send(resp);
    });

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

    char p[200];
    snprintf(p, sizeof(p), "METRICS:%d|%d|%d|%d|%.1f|%s|%d|%u|%d|%d|%.2f|%.2f|%d|%.3f",
             physicsLite.getWatts(), (int)physicsLite.getTotalDistance(), physicsLite.getSpm(), 
             (int)physicsLite.getCurrentPace(), physicsLite.getTotalKcal(), 
             (physicsLite.getIsPulling() ? "1" : "0"), 
             (int)loadCell.getLastRaw(), 0, 0, 0, physicsLite.getCurrentForce(), 0.0f, physicsLite.getIsPulling() ? 1 : 0, 0.0f);
    ws.textAll(p);
}
