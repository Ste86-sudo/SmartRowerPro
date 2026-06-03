#include "WebUI.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "SharedData.h"
#include "ConfigManager.h"
#include "WebUI_HTML.h"

WebUI webApp;

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

void onWsEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
    if (t == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)a;
        if (info->final && info->index == 0 && info->len == l && info->opcode == WS_TEXT) {
            d[l] = 0; String cmd = String((char*)d); cmd.trim(); 
            
            if (cmd == "tare") {
                config.saveTara(metrics.rawAdc);
            } else if (cmd.startsWith("calib:")) {
                float ref = cmd.substring(6).toFloat();
                if (ref > 0.01) {
                    float delta = (float)metrics.rawAdc - config.tara;
                    if (fabs(delta) > 1.0) config.saveScale(fabs(delta) / ref);
                }
            } else if (cmd == "SCAN") {
                WiFi.scanNetworks(true, true); // async scan, show hidden
            } else if (cmd == "REBOOT") {
                delay(500); ESP.restart();
            } else if (cmd.startsWith("CFG:")) {
            // CFG:tara,scala,uHeight,uWeight,uPull,uRel,encPPR,pullCirc,laserOffset,wifiSSID,wifiPass
            int c1 = cmd.indexOf(',');
            int c2 = cmd.indexOf(',', c1+1);
            int c3 = cmd.indexOf(',', c2+1);
            int c4 = cmd.indexOf(',', c3+1);
            int c5 = cmd.indexOf(',', c4+1);
            int c6 = cmd.indexOf(',', c5+1);
            int c7 = cmd.indexOf(',', c6+1);
            int c8 = cmd.indexOf(',', c7+1);
            int c9 = cmd.indexOf(',', c8+1);
            int c10 = cmd.indexOf(',', c9+1);

            if (c1>0 && c7>0) {
                int32_t tara = cmd.substring(4, c1).toInt();
                float scala = cmd.substring(c1+1, c2).toFloat();
                float uh = cmd.substring(c2+1, c3).toFloat();
                float uw = cmd.substring(c3+1, c4).toFloat();
                float up = cmd.substring(c4+1, c5).toFloat();
                float ur = cmd.substring(c5+1, c6).toFloat();
                float eppr = cmd.substring(c6+1, c7).toFloat();
                
                float pcirc = config.pullCirc;
                float lOffset = config.laserOffset;
                String wSsid = config.wifiSSID;
                String wPass = config.wifiPass;

                if (c8 > 0) pcirc = cmd.substring(c7+1, c8).toFloat();
                if (c9 > 0) lOffset = cmd.substring(c8+1, c9).toFloat();
                if (c10 > 0) {
                    wSsid = cmd.substring(c9+1, c10);
                    wPass = cmd.substring(c10+1);
                } else if (c9 > 0 && c10 == -1) {
                    wSsid = cmd.substring(c9+1);
                    wPass = "";
                }

                config.saveTara(tara);
                config.saveScale(scala);
                config.saveProfile(uh, uw, up, ur);
                config.saveMechanics(eppr, pcirc, lOffset);
                config.saveWiFi(wSsid, wPass);
                
                ws.textAll("CFG:" + String(config.tara) + "," + String(config.scala) + "," + String(config.uHeight,1) + "," + 
                String(config.uWeight,1) + "," + String(config.pullThresh,1) + "," + String(config.relThresh,1) + "," + 
                String(config.encPPR) + "," + String(config.pullCirc) + "," + String(config.laserOffset) + "," + 
                config.wifiSSID + "," + config.wifiPass + "|" + WiFi.softAPmacAddress());
            }
        }
        }
    }
    if (t == WS_EVT_CONNECT) {
        c->text("CFG:" + String(config.tara) + "," + String(config.scala) + "," + String(config.uHeight,1) + "," + 
                String(config.uWeight,1) + "," + String(config.pullThresh,1) + "," + String(config.relThresh,1) + "," + 
                String(config.encPPR) + "," + String(config.pullCirc) + "," + String(config.laserOffset) + "," + 
                config.wifiSSID + "," + config.wifiPass + "|" + WiFi.softAPmacAddress());
    }
}

void WebUI::begin() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ 
        AsyncWebServerResponse *response = r->beginResponse_P(200, "text/html", index_html_gz, sizeof(index_html_gz));
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        r->send(response);
    });
    
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *r){
        r->send(200, "text/html", "<!DOCTYPE html><html><body style='background:#020617;color:#fff;text-align:center;'><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='FLASH'></form></body></html>");
    });
    
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *r){
        bool ok = !Update.hasError();
        AsyncWebServerResponse *res = r->beginResponse(200, "text/plain", ok ? "OK" : "ERR");
        res->addHeader("Connection", "close"); r->send(res);
        if(ok) { delay(500); ESP.restart(); }
    }, [](AsyncWebServerRequest *r, String f, size_t i, uint8_t *d, size_t l, bool fin) {
        if(!i) Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
        if(!Update.hasError()) Update.write(d, l); if(fin) Update.end(true);
    });

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
}

void WebUI::handleClients() {
    ws.cleanupClients(1);
    
    int16_t n = WiFi.scanComplete();
    if (n >= 0) {
        String res = "WIFI_LIST:";
        for (int i = 0; i < n; ++i) {
            if (i > 0) res += ",";
            res += WiFi.SSID(i);
        }
        ws.textAll(res);
        WiFi.scanDelete();
    }
}

void WebUI::sendTelemetryBuffer(volatile float* readyBuffer) {
    if (readyBuffer != nullptr) {
        if(ws.count() > 0 && ws.availableForWriteAll()) {
            char p[1500]; 
            int o = snprintf(p, 1500, "SYNC:");
            for(int i=0; i<150; i++) {
                o += snprintf(p+o, 1500-o, "%.2f%s", readyBuffer[i], (i==149?"":","));
            }
            ws.textAll(p);
            syncPacketCounter++;
        }
        // Il buffer deve essere pulito da chi lo gestisce (PhysicsEngine), 
        // per questo chiamiamo clearReadyBuffer() dal loop principale
    }
}

void WebUI::sendMetrics() {
    if(ws.count() > 0) {
        char p[150];
        snprintf(p, 150, "METRICS:%d|%d|%d|%d|%.1f|%s|%d|%u|%d",
                 metrics.watts, (int)metrics.totalDistance, metrics.spm, metrics.paceSeconds, 
                 metrics.totalKcal, (telemetry.active ? "1" : "0"), metrics.rawAdc, syncPacketCounter, telemetry.heartRate);
        ws.textAll(p);
    }
}