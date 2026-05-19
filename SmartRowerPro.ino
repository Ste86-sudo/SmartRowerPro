#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <SPI.h>
#include <ADS1220_WE.h> 
#include <Preferences.h> 

// --- LIBRERIE BLUETOOTH (BLE) ---
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- Configurazione Hardware ESP32 ---
#define SPI_MISO          0
#define SPI_MOSI          1
#define SPI_SCK           2
#define ADS1220_CS_PIN    3
#define ADS_CLK_PIN       4  
#define ADS1220_DRDY_PIN  5

// --- Wi-Fi SoftAP ---
const char* ssid = "RP_AP";
const char* password = "password"; 

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
ADS1220_WE ads = ADS1220_WE(ADS1220_CS_PIN, ADS1220_DRDY_PIN);
Preferences prefs;

// --- Variabili Sensore e Profilo Persistenti ---
float forzaKg = 0.0;
int32_t valoreTara = 0;
float fattoreDiScala = 10000.0;
float userHeight = 181.0; 
float userWeight = 75.0;  
float pullThreshold = 3.0;    
float releaseThreshold = 1.5; 

// --- Variabili BLE ---
BLEServer* pServer = NULL;
BLECharacteristic* pFtmsRowerData = NULL; 
bool deviceConnected = false;

// Profilo UNICO: Fitness Machine Service (Vogatore per EXR/Kinomap)
#define FTMS_SERVICE_UUID         "1826"
#define ROWER_DATA_UUID           "2AD1"
#define FTMS_FEATURE_UUID         "2ACC"
#define FTMS_CONTROL_POINT_UUID   "2AD9" 

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; Serial.println("App BLE Connessa!"); }
    void onDisconnect(BLEServer* pServer) { 
        deviceConnected = false;
        Serial.println("App BLE Disconnessa. Riavvio trasmissione..."); 
        BLEDevice::startAdvertising(); 
    }
};

class FTMSControlPointCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        uint8_t* data = pCharacteristic->getData();
        size_t len = pCharacteristic->getLength();
        if (len > 0) {
            uint8_t opCode = data[0];
            uint8_t response[3] = {0x80, opCode, 0x01};
            pCharacteristic->setValue(response, 3);
            pCharacteristic->notify();
        }
    }
};

// =================================================================================
// WEB APP PRO 
// =================================================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Smart Rower Pro</title>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <link rel="icon" href="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAxMDAgMTAwIj48cmVjdCB3aWR0aD0iMTAwIiBoZWlnaHQ9IjEwMCIgcng9IjIwIiBmaWxsPSIjMDIwNjE3Ii8+PHBhdGggZD0iTTIwIDcwIFEgMzUgNTAgNTAgNzAgVCA4MCA3MCIgZmlsbD0ibm9uZSIgc3Ryb2tlPSIjMjJkM2VlIiBzdHJva2Utd2lkdGg9IjgiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIvPjxwYXRoIGQ9Ik0zMCA0MCBMIDUwIDYwIEwgNzAgNDAiIGZpbGw9Im5vbmUiIHN0cm9rZT0iI2VmNDQ0NCIgc3Ryb2tlLXdpZHRoPSI4IiBzdHJva2UtbGluZWNhcD0icm91bmQiIHN0cm9rZS1saW5lam9pbj0icm91bmQiLz48Y2lyY2xlIGN4PSI1MCIgY3k9IjI1IiByPSI4IiBmaWxsPSIjMTBiOTgxIi8+PC9zdmc+" type="image/svg+xml">
    
    <style>
        :root { --bg: #020617; --panel: #1e293b; --accent: #22d3ee; --work: #ef4444; --rest: #10b981; --text: #f8fafc; --power: #f59e0b; --dist: #a855f7; }
        body { font-family: 'Inter', system-ui, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 10px; display: flex; flex-direction: column; align-items: center; }
        .container { width: 100%; max-width: 900px; position: relative; }
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; padding: 10px 20px; background: var(--panel); border-radius: 12px; border-bottom: 2px solid var(--accent); }
        .title { margin: 0; font-size: 1.5em; font-weight: 900; color: var(--accent); letter-spacing: 1px; }
        .tabs { display: flex; background: #0f172a; border-radius: 12px; margin-bottom: 15px; overflow: hidden; border: 1px solid #334155; }
        .tab-btn { flex: 1; padding: 15px; text-align: center; font-weight: bold; cursor: pointer; transition: 0.3s; color: #64748b; border-bottom: 3px solid transparent; }
        .tab-btn.active { color: var(--accent); border-bottom: 3px solid var(--accent); background: #1e293b; }
        .tab-content { display: none; }
        .tab-content.active { display: block; }
        .card { background: var(--panel); padding: 20px; border-radius: 12px; border: 1px solid #334155; margin-bottom: 15px; }
        .grid-3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 15px; }
        .grid-2 { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin-bottom: 15px; }
        .grid-4 { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; margin-bottom: 15px; }
        .label { font-size: 0.8rem; color: #94a3b8; text-transform: uppercase; margin-bottom: 5px; text-align: center; }
        .val { font-size: 2.5rem; font-weight: 900; color: white; text-align: center; }
        .unit { font-size: 0.9rem; color: #64748b; }
        input, select { width: 100%; box-sizing: border-box; background: #0f172a; border: 1px solid #475569; color: white; padding: 12px; border-radius: 8px; font-size: 1.1rem; text-align: center; margin-bottom: 15px; }
        .btn { width: 100%; padding: 15px; border: none; border-radius: 8px; font-weight: bold; font-size: 1.1rem; cursor: pointer; transition: 0.2s; }
        .btn-primary { background: var(--accent); color: #000; box-shadow: 0 0 15px rgba(34,211,238,0.3); }
        .btn-danger { background: #7f1d1d; color: white; }
        .btn-outline { background: transparent; border: 2px solid #475569; color: white; }
        .workout-banner { background: #000; padding: 15px; border-radius: 12px; text-align: center; border: 2px solid var(--accent); margin-bottom: 15px; }
        .phase-text { font-size: 1.5rem; font-weight: 900; letter-spacing: 2px; }
        .chart-box { background: #000; border-radius: 12px; padding: 10px; border: 1px solid #334155; position: relative; }
        canvas { width: 100%; height: 250px; display: block; }
        
        .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.85); z-index: 1000; justify-content: center; align-items: center; padding: 20px; box-sizing: border-box; backdrop-filter: blur(5px); }
        .modal-content { background: var(--panel); padding: 30px; border-radius: 15px; border: 2px solid var(--accent); width: 100%; max-width: 450px; text-align: center; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }
        .modal h2 { margin-top: 0; color: var(--accent); letter-spacing: 1px; font-weight: 900; margin-bottom: 25px; }
        .stat-row { display: flex; justify-content: space-between; padding: 12px 0; border-bottom: 1px solid #334155; font-size: 1.1rem; }
        .stat-row:last-child { border-bottom: none; }
        .stat-val { font-weight: 900; color: white; font-size: 1.2rem; }
        
        @media (max-width: 600px) { .grid-3, .grid-4, .grid-2 { grid-template-columns: 1fr; } .header { flex-direction: column; gap: 10px; text-align: center; } }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1 class="title">SMART ROWER PRO</h1>
            <div style="display: flex; align-items: center; gap: 15px;">
                <span style="font-size:0.8em; color:var(--text-dim);" id="wsStatus">Connecting...</span>
                <button class="btn btn-outline" style="padding: 8px 15px; font-size: 0.8rem; width: auto;" onclick="window.location.href='/update'">OTA UPDATE</button>
            </div>
        </div>

        <div class="tabs">
            <div class="tab-btn active" onclick="switchTab('dashboard')">DASHBOARD</div>
            <div class="tab-btn" onclick="switchTab('workout')">ALLENAMENTO</div>
            <div class="tab-btn" onclick="switchTab('calib')">SETUP & PROFILO</div>
        </div>

        <div id="tab-dashboard" class="tab-content active">
            <div class="workout-banner" id="bannerWorkout" style="display:none;">
                <div class="phase-text" id="wPhase" style="color:var(--accent);">WARMUP</div>
                <div style="font-size:3rem; font-weight:900; font-family:monospace;" id="wTimer">00:00</div>
            </div>

            <div class="grid-3">
                <div class="card" style="padding:15px;"><div class="label">Forza Live</div><div class="val" id="fLive">0.0<span class="unit">kg</span></div></div>
                <div class="card" style="padding:15px;"><div class="label">Ritmo SPM</div><div class="val" id="fSpm">0</div><div id="tgtSpm" class="label" style="color:var(--accent)"></div></div>
                <div class="card" style="padding:15px;"><div class="label">Picco Tirata</div><div class="val" id="fPeak">0.0<span class="unit">kg</span></div><div id="tgtPeak" class="label" style="color:var(--accent)"></div></div>
            </div>

            <div class="chart-box">
                <canvas id="rowCanvas"></canvas>
            </div>

            <div class="grid-3" style="margin-top:15px;">
                <div class="card" style="padding:15px;"><div class="label">Potenza Pagaiata</div><div class="val" id="sWatts" style="color:var(--power)">0<span class="unit">W</span></div></div>
                <div class="card" style="padding:15px;"><div class="label">Passo /500m</div><div class="val" id="sPace" style="color:var(--dist)">--:--</div></div>
                <div class="card" style="padding:15px;"><div class="label">Distanza</div><div class="val" id="sDist" style="color:var(--dist)">0<span class="unit">m</span></div></div>
            </div>
            
            <div class="grid-2">
                <div class="card" style="padding:15px;"><div class="label">Totale Colpi</div><div class="val" id="sTotal">0</div></div>
                <div class="card" style="padding:15px;"><div class="label">Calorie Biologiche</div><div class="val" id="sKcal" style="color:#fb7185">0.0<span class="unit">kcal</span></div></div>
            </div>
        </div>

        <div id="tab-workout" class="tab-content">
            <div class="card">
                <h3 style="margin-top:0;">Scegli il Tipo di Allenamento</h3>
                <select id="wType" onchange="updateWorkoutUI()">
                    <option value="free">Stile Libero (Nessun Target)</option>
                    <option value="target">Target Fisso (Cerca la pagaiata perfetta)</option>
                    <option value="interval">Interval Training (HIIT Base)</option>
                    <option value="hiit_adv">HIIT Advanced (Custom Multi-Serie)</option>
                    <option value="tabata">Tabata Custom</option>
                    <option value="pyramid">Piramide Custom</option>
                </select>

                <div id="ui-target" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-3">
                        <div><div class="label">Target SPM</div><input type="number" id="tSpm" value="24" oninput="liveUpdateTargets()"></div>
                        <div><div class="label">Target Forza (Kg)</div><input type="number" id="tForce" value="30" oninput="liveUpdateTargets()"></div>
                        <div><div class="label">Duty Cycle Tirata (%)</div><input type="number" id="tDuty" value="35" oninput="liveUpdateTargets()"></div>
                    </div>
                </div>

                <div id="ui-interval" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-4">
                        <div><div class="label" style="color:var(--accent)">Warmup (sec)</div><input type="number" id="iWarm" value="60"></div>
                        <div><div class="label" style="color:var(--accent)">Ripetizioni</div><input type="number" id="iReps" value="8"></div>
                        <div><div class="label" style="color:var(--accent)">Duty Tirata (%)</div><input type="number" id="iWarmDuty" value="30"></div>
                        <div></div> 
                        
                        <div><div class="label" style="color:var(--work)">Sprint (sec)</div><input type="number" id="iWork" value="30"></div>
                        <div><div class="label" style="color:var(--work)">Target SPM</div><input type="number" id="iWorkSpm" value="28"></div>
                        <div><div class="label" style="color:var(--work)">Target Kg</div><input type="number" id="iWorkKg" value="40"></div>
                        <div><div class="label" style="color:var(--work)">Duty Tirata (%)</div><input type="number" id="iWorkDuty" value="40"></div>
                        
                        <div><div class="label" style="color:var(--rest)">Recupero (sec)</div><input type="number" id="iRest" value="30"></div>
                        <div><div class="label" style="color:var(--rest)">Target SPM</div><input type="number" id="iRestSpm" value="16"></div>
                        <div><div class="label" style="color:var(--rest)">Target Kg</div><input type="number" id="iRestKg" value="15"></div>
                        <div><div class="label" style="color:var(--rest)">Duty Tirata (%)</div><input type="number" id="iRestDuty" value="25"></div>
                    </div>
                </div>

                <div id="ui-hiit-adv" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-4">
                        <div><div class="label" style="color:var(--accent)">Warmup (sec)</div><input type="number" id="haWarm" value="180"></div>
                        <div><div class="label" style="color:#3b82f6">Numero Serie (Sets)</div><input type="number" id="haSets" value="3"></div>
                        <div><div class="label" style="color:var(--accent)">Reps per Serie</div><input type="number" id="haReps" value="5"></div>
                        <div><div class="label" style="color:#3b82f6">Recupero tra Serie (sec)</div><input type="number" id="haSetRest" value="120"></div>

                        <div><div class="label" style="color:var(--work)">Sprint (sec)</div><input type="number" id="haWork" value="40"></div>
                        <div><div class="label" style="color:var(--work)">Target SPM</div><input type="number" id="haWorkSpm" value="28"></div>
                        <div><div class="label" style="color:var(--work)">Target Kg</div><input type="number" id="haWorkKg" value="45"></div>
                        <div></div> 

                        <div><div class="label" style="color:var(--rest)">Recupero Corto (sec)</div><input type="number" id="haRest" value="20"></div>
                        <div><div class="label" style="color:var(--rest)">Target SPM</div><input type="number" id="haRestSpm" value="16"></div>
                        <div><div class="label" style="color:var(--rest)">Target Kg</div><input type="number" id="haRestKg" value="15"></div>
                        <div></div> 
                    </div>
                </div>

                <div id="ui-tabata" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-4">
                        <div><div class="label" style="color:var(--accent)">Warmup (sec)</div><input type="number" id="tbWarm" value="180"></div>
                        <div><div class="label" style="color:var(--accent)">Ripetizioni</div><input type="number" id="tbReps" value="8"></div>
                        <div></div><div></div>

                        <div><div class="label" style="color:var(--work)">Sprint (sec)</div><input type="number" id="tbWork" value="20"></div>
                        <div><div class="label" style="color:var(--work)">Target SPM</div><input type="number" id="tbWorkSpm" value="32"></div>
                        <div><div class="label" style="color:var(--work)">Target Kg</div><input type="number" id="tbWorkKg" value="40"></div>
                        <div></div>

                        <div><div class="label" style="color:var(--rest)">Recupero (sec)</div><input type="number" id="tbRest" value="10"></div>
                        <div><div class="label" style="color:var(--rest)">Target SPM</div><input type="number" id="tbRestSpm" value="16"></div>
                        <div><div class="label" style="color:var(--rest)">Target Kg</div><input type="number" id="tbRestKg" value="10"></div>
                        <div></div>
                    </div>
                </div>

                <div id="ui-pyramid" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-4">
                        <div><div class="label" style="color:var(--accent)">Warmup (sec)</div><input type="number" id="pyWarm" value="180"></div>
                        <div><div class="label" style="color:var(--work)">Step 1 (min)</div><input type="number" id="pyS1" value="1"></div>
                        <div><div class="label" style="color:var(--work)">Step 2 (min)</div><input type="number" id="pyS2" value="2"></div>
                        <div><div class="label" style="color:var(--work)">Step 3 (min)</div><input type="number" id="pyS3" value="3"></div>

                        <div><div class="label" style="color:var(--work)">Target SPM</div><input type="number" id="pyWorkSpm" value="24"></div>
                        <div><div class="label" style="color:var(--work)">Target Kg</div><input type="number" id="pyWorkKg" value="35"></div>
                        <div><div class="label" style="color:var(--rest)">Recup. tra step (sec)</div><input type="number" id="pyRest" value="60"></div>
                        <div></div>
                        
                        <div><div class="label" style="color:var(--rest)">Recupero SPM</div><input type="number" id="pyRestSpm" value="16"></div>
                        <div><div class="label" style="color:var(--rest)">Recupero Kg</div><input type="number" id="pyRestKg" value="15"></div>
                        <div></div><div></div>
                    </div>
                </div>

                <button class="btn btn-primary" onclick="startWorkout()" style="margin-top:15px;">INIZIA ALLENAMENTO</button>
            </div>
            <button class="btn btn-danger" onclick="endWorkout()">FINE ALLENAMENTO / VEDI STATISTICHE</button>
        </div>

        <div id="tab-calib" class="tab-content">
            <div class="card">
                <h3 style="margin-top:0; color:var(--accent);">Profilo Utente & Sensibilità Pagaiata</h3>
                <p style="color:#94a3b8; font-size:0.9rem;">Imposta i tuoi dati fisici e le soglie di forza (Kg) per il riconoscimento dei colpi.</p>
                <div class="grid-2">
                    <div><div class="label">Altezza (cm)</div><input type="number" id="uHeight" value="181"></div>
                    <div><div class="label">Peso (Kg)</div><input type="number" id="uWeight" value="75"></div>
                    <div><div class="label" style="color:var(--accent)">Soglia Inizio Tiro (Kg)</div><input type="number" id="uPull" value="3.0" step="0.1"></div>
                    <div><div class="label" style="color:var(--accent)">Soglia Ritorno (Kg)</div><input type="number" id="uRel" value="1.5" step="0.1"></div>
                </div>
                <button class="btn btn-outline" onclick="saveProfile()" style="border-color:var(--accent); color:var(--accent);">SALVA PROFILO E SOGLIE</button>
                <div id="profMsg" style="color:#10b981; font-weight:bold; text-align:center; height:20px; margin-top:5px;"></div>
            </div>

            <div class="card">
                <h3 style="margin-top:0; color:var(--accent);">Step 1: Azzeramento (Tara)</h3>
                <button class="btn btn-outline" onclick="sendCommand('tare')">IMPOSTA ZERO</button>
            </div>

            <div class="card">
                <h3 style="margin-top:0; color:var(--accent);">Step 2: Calibrazione Peso</h3>
                <div class="label" style="text-align:left;">Peso applicato (Kg):</div>
                <input type="number" id="calibWeight" value="10.0" step="0.1">
                <button class="btn btn-primary" onclick="calibrateScale()">CALIBRA SENSORE</button>
            </div>
            <div id="calibMsg" style="color:#10b981; font-weight:bold; text-align:center; height:20px;"></div>
        </div>
    </div>
    
    <div id="statsModal" class="modal">
        <div class="modal-content">
            <h2>ALLENAMENTO COMPLETATO</h2>
            <div class="stat-row"><span>Tempo Totale</span><span id="mTime" class="stat-val">00:00</span></div>
            <div class="stat-row"><span>Distanza</span><span id="mDist" class="stat-val">0 m</span></div>
            <div class="stat-row"><span>Colpi Totali</span><span id="mStrokes" class="stat-val">0</span></div>
            <div class="stat-row"><span>Passo Medio (/500m)</span><span id="mPace" class="stat-val">--:--</span></div>
            <div class="stat-row"><span>Potenza Media</span><span id="mAvgWatts" class="stat-val" style="color:var(--power)">0 W</span></div>
            <div class="stat-row"><span>Potenza Picco</span><span id="mMaxWatts" class="stat-val" style="color:var(--power)">0 W</span></div>
            <div class="stat-row"><span>Calorie</span><span id="mKcal" class="stat-val" style="color:#fb7185">0.0 kcal</span></div>
            <button class="btn btn-primary" style="margin-top:25px;" onclick="closeStats()">CHIUDI E TORNA ALLA DASHBOARD</button>
        </div>
    </div>

    <script>
        function initStorage() {
            const fieldsToSave = [
                'wType', 'tSpm', 'tForce', 'tDuty', 
                'iWarm', 'iReps', 'iWarmDuty', 
                'iWork', 'iWorkSpm', 'iWorkKg', 'iWorkDuty', 
                'iRest', 'iRestSpm', 'iRestKg', 'iRestDuty',
                'haWarm', 'haSets', 'haReps', 'haSetRest',
                'haWork', 'haWorkSpm', 'haWorkKg',
                'haRest', 'haRestSpm', 'haRestKg',
                'tbWarm', 'tbReps', 'tbWork', 'tbWorkSpm', 'tbWorkKg', 'tbRest', 'tbRestSpm', 'tbRestKg',
                'pyWarm', 'pyS1', 'pyS2', 'pyS3', 'pyWorkSpm', 'pyWorkKg', 'pyRest', 'pyRestSpm', 'pyRestKg'
            ];
            
            fieldsToSave.forEach(fieldId => {
                let el = document.getElementById(fieldId);
                if(el) {
                    let savedVal = localStorage.getItem('rower_pref_' + fieldId);
                    if(savedVal !== null) {
                        el.value = savedVal;
                    }
                    el.addEventListener('input', () => {
                        localStorage.setItem('rower_pref_' + fieldId, el.value);
                    });
                    if(fieldId === 'wType') {
                        el.addEventListener('change', updateWorkoutUI);
                    }
                }
            });
        }

        function switchTab(tabId) {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            if(event && event.target) event.target.classList.add('active');
            document.getElementById('tab-' + tabId).classList.add('active');
        }

        function updateWorkoutUI() {
            let t = document.getElementById('wType').value;
            document.getElementById('ui-target').style.display = (t === 'target') ? 'block' : 'none';
            document.getElementById('ui-interval').style.display = (t === 'interval') ? 'block' : 'none';
            document.getElementById('ui-hiit-adv').style.display = (t === 'hiit_adv') ? 'block' : 'none';
            document.getElementById('ui-tabata').style.display = (t === 'tabata') ? 'block' : 'none';
            document.getElementById('ui-pyramid').style.display = (t === 'pyramid') ? 'block' : 'none';
        }

        function liveUpdateTargets() {
            if(mode === 'target') {
                targetSPM = parseInt(document.getElementById('tSpm').value) || 0;
                targetPeak = parseFloat(document.getElementById('tForce').value) || 0;
                targetDuty = parseInt(document.getElementById('tDuty').value) || 35;
                updateTargetLabels();
            }
        }

        let gateway = `ws://${window.location.hostname}/ws`;
        let websocket;
        let history = [];
        let isPulling = false;
        let currentPeak = 0;
        let lastStrokeTime = 0;
        
        // JS Thresholds
        let pullThresh = 3.0;
        let relThresh = 1.5;
        
        // Workout Stats
        let totalStrokes = 0;
        let totalKcal = 0.0;
        let sumWatts = 0;
        let maxStrokeWatts = 0;
        let totalDistance = 0.0;
        let workoutStartMs = 0;
        
        let frameCounter = 0;
        let drawPending = false; 
        
        let mode = 'free'; 
        let targetSPM = 0;
        let targetPeak = 0;
        let targetDuty = 35;
        
        // Timer Interval
        let itRunning = false;
        let itTimeLeft = 0;
        let itStages = []; 
        let itStageIndex = 0;
        let itInterval;
        let globalTotalTime = 0;
        let globalTimeElapsed = 0;
        
        const canvas = document.getElementById('rowCanvas');
        const ctx = canvas.getContext('2d');
        
        function startWorkout() {
            mode = document.getElementById('wType').value;
            totalStrokes = 0;
            totalKcal = 0; sumWatts = 0; totalDistance = 0.0; maxStrokeWatts = 0;
            workoutStartMs = Date.now();
            
            document.getElementById('sTotal').innerText = "0";
            document.getElementById('sWatts').innerHTML = "0<span class='unit'>W</span>";
            document.getElementById('sKcal').innerHTML = "0.0<span class='unit'>kcal</span>";
            document.getElementById('sDist').innerHTML = "0<span class='unit'>m</span>";
            document.getElementById('sPace').innerText = "--:--";
            document.getElementById('fSpm').innerText = "0";
            
            if(mode === 'free') { 
                targetSPM = 0; targetPeak = 0; targetDuty = 35;
                document.getElementById('bannerWorkout').style.display = 'none'; 
            } else if (mode === 'target') { 
                targetSPM = parseInt(document.getElementById('tSpm').value);
                targetPeak = parseFloat(document.getElementById('tForce').value); 
                targetDuty = parseInt(document.getElementById('tDuty').value) || 35;
                document.getElementById('bannerWorkout').style.display = 'none';
            } else { 
                document.getElementById('bannerWorkout').style.display = 'block';
                setupIntervals(); 
            }
            
            updateTargetLabels();
            let dashBtn = document.querySelector('.tab-btn:first-child');
            if(dashBtn) {
                document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
                dashBtn.classList.add('active');
            }
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            document.getElementById('tab-dashboard').classList.add('active');
        }

        function endWorkout() {
            itRunning = false;
            clearInterval(itInterval);
            
            if (totalStrokes > 0) {
                let totalSecs = Math.floor((Date.now() - workoutStartMs) / 1000);
                let m = Math.floor(totalSecs / 60);
                let s = totalSecs % 60;
                
                document.getElementById('mTime').innerText = `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`;
                document.getElementById('mDist').innerText = `${Math.round(totalDistance)} m`;
                document.getElementById('mStrokes').innerText = totalStrokes;
                document.getElementById('mKcal').innerText = `${totalKcal.toFixed(1)} kcal`;
                
                let avgW = Math.round(sumWatts / totalStrokes);
                document.getElementById('mAvgWatts').innerText = `${avgW} W`;
                document.getElementById('mMaxWatts').innerText = `${Math.round(maxStrokeWatts)} W`;
                
                let avgSpeed = totalDistance / totalSecs;
                if(avgSpeed > 0) {
                    let pace = 500.0 / avgSpeed;
                    let pm = Math.floor(pace / 60);
                    let ps = Math.floor(pace % 60);
                    document.getElementById('mPace').innerText = `${pm}:${String(ps).padStart(2,'0')}`;
                } else {
                    document.getElementById('mPace').innerText = `--:--`;
                }
                
                document.getElementById('statsModal').style.display = 'flex';
            } else {
                closeStats(); 
            }
        }
        
        function closeStats() {
            document.getElementById('statsModal').style.display = 'none';
            mode = 'free';
            targetSPM = 0; targetPeak = 0; targetDuty = 35; 
            document.getElementById('bannerWorkout').style.display = 'none';
            updateTargetLabels();
            
            let dashBtn = document.querySelector('.tab-btn:first-child');
            if(dashBtn) {
                document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
                dashBtn.classList.add('active');
            }
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            document.getElementById('tab-dashboard').classList.add('active');
        }

        function setupIntervals() {
            itStages = [];
            
            if (mode === 'interval') {
                const w = parseInt(document.getElementById('iWarm').value);
                const work = parseInt(document.getElementById('iWork').value);
                const rest = parseInt(document.getElementById('iRest').value);
                const reps = parseInt(document.getElementById('iReps').value);
                const wSpm = parseInt(document.getElementById('iWorkSpm').value);
                const wKg = parseFloat(document.getElementById('iWorkKg').value);
                const rSpm = parseInt(document.getElementById('iRestSpm').value);
                const rKg = parseFloat(document.getElementById('iRestKg').value);
                const iWarmDuty = parseInt(document.getElementById('iWarmDuty').value);
                const iWorkDuty = parseInt(document.getElementById('iWorkDuty').value);
                const iRestDuty = parseInt(document.getElementById('iRestDuty').value);
                
                itStages.push({name: 'WARMUP', dur: w, spm: 18, peak: 15, duty: iWarmDuty, col: '#22d3ee'});
                for(let i=0; i<reps; i++) {
                    itStages.push({name: `SFORZO MASSIMO ${i+1}/${reps}`, dur: work, spm: wSpm, peak: wKg, duty: iWorkDuty, col: '#ef4444'});
                    itStages.push({name: `RECUPERO ATTIVO ${i+1}/${reps}`, dur: rest, spm: rSpm, peak: rKg, duty: iRestDuty, col: '#10b981'});
                }
                itStages.push({name: 'COOLDOWN', dur: 60, spm: 16, peak: 10, duty: iWarmDuty, col: '#22d3ee'});
            }
            else if (mode === 'hiit_adv') {
                const w = parseInt(document.getElementById('haWarm').value);
                const sets = parseInt(document.getElementById('haSets').value);
                const reps = parseInt(document.getElementById('haReps').value);
                const setRest = parseInt(document.getElementById('haSetRest').value);
                const work = parseInt(document.getElementById('haWork').value);
                const workSpm = parseInt(document.getElementById('haWorkSpm').value);
                const workKg = parseFloat(document.getElementById('haWorkKg').value);
                const rest = parseInt(document.getElementById('haRest').value);
                const restSpm = parseInt(document.getElementById('haRestSpm').value);
                const restKg = parseFloat(document.getElementById('haRestKg').value);

                itStages.push({name: 'WARMUP', dur: w, spm: 18, peak: 15, duty: 30, col: '#22d3ee'});
                
                for(let s=1; s<=sets; s++) {
                    for(let r=1; r<=reps; r++) {
                        itStages.push({name: `SERIE ${s} - SPRINT ${r}/${reps}`, dur: work, spm: workSpm, peak: workKg, duty: 40, col: '#ef4444'});
                        if (r < reps) {
                            itStages.push({name: `SERIE ${s} - RECUPERO CORT. ${r}/${reps}`, dur: rest, spm: restSpm, peak: restKg, duty: 25, col: '#10b981'});
                        }
                    }
                    if (s < sets) {
                        itStages.push({name: `RECUPERO LUNGO (TRA SERIE ${s} E ${s+1})`, dur: setRest, spm: 16, peak: 10, duty: 25, col: '#3b82f6'});
                    }
                }
                itStages.push({name: 'COOLDOWN', dur: 120, spm: 16, peak: 10, duty: 30, col: '#22d3ee'});
            }
            else if (mode === 'tabata') {
                const w = parseInt(document.getElementById('tbWarm').value);
                const reps = parseInt(document.getElementById('tbReps').value);
                const work = parseInt(document.getElementById('tbWork').value);
                const wSpm = parseInt(document.getElementById('tbWorkSpm').value);
                const wKg = parseFloat(document.getElementById('tbWorkKg').value);
                const rest = parseInt(document.getElementById('tbRest').value);
                const rSpm = parseInt(document.getElementById('tbRestSpm').value);
                const rKg = parseFloat(document.getElementById('tbRestKg').value);

                itStages.push({name: 'WARMUP', dur: w, spm: 18, peak: 15, duty: 30, col: '#22d3ee'});
                for(let i=1; i<=reps; i++) {
                    itStages.push({name: `TABATA SPRINT ${i}/${reps}`, dur: work, spm: wSpm, peak: wKg, duty: 40, col: '#ef4444'});
                    itStages.push({name: `TABATA REST ${i}/${reps}`, dur: rest, spm: rSpm, peak: rKg, duty: 25, col: '#10b981'});
                }
                itStages.push({name: 'COOLDOWN', dur: 120, spm: 16, peak: 10, duty: 30, col: '#22d3ee'});
            }
            else if (mode === 'pyramid') {
                const w = parseInt(document.getElementById('pyWarm').value);
                const s1 = parseInt(document.getElementById('pyS1').value);
                const s2 = parseInt(document.getElementById('pyS2').value);
                const s3 = parseInt(document.getElementById('pyS3').value);
                const wSpm = parseInt(document.getElementById('pyWorkSpm').value);
                const wKg = parseFloat(document.getElementById('pyWorkKg').value);
                const rest = parseInt(document.getElementById('pyRest').value);
                const rSpm = parseInt(document.getElementById('pyRestSpm').value);
                const rKg = parseFloat(document.getElementById('pyRestKg').value);

                itStages.push({name: 'WARMUP', dur: w, spm: 18, peak: 15, duty: 30, col: '#22d3ee'});
                const steps = [s1, s2, s3, s2, s1]; 
                for(let i=0; i<steps.length; i++) {
                    itStages.push({name: `PIRAMIDE SFORZO (${steps[i]} min)`, dur: steps[i]*60, spm: wSpm, peak: wKg, duty: 35, col: '#f59e0b'});
                    if(i < steps.length - 1) {
                        itStages.push({name: 'RECUPERO', dur: rest, spm: rSpm, peak: rKg, duty: 25, col: '#10b981'});
                    }
                }
                itStages.push({name: 'COOLDOWN', dur: 120, spm: 16, peak: 10, duty: 30, col: '#22d3ee'});
            }

            globalTotalTime = 0;
            for(let s of itStages) globalTotalTime += s.dur;
            globalTimeElapsed = 0;

            itStageIndex = 0; itRunning = true;
            itTimeLeft = itStages[0].dur;
            runTick(); 
            itInterval = setInterval(runTick, 1000);
        }

        function runTick() {
            if(itTimeLeft <= 0) {
                itStageIndex++;
                if(itStageIndex >= itStages.length) { endWorkout(); return; }
                itTimeLeft = itStages[itStageIndex].dur;
            }
            let stage = itStages[itStageIndex];
            document.getElementById('wPhase').innerText = stage.name;
            document.getElementById('wPhase').style.color = stage.col;
            document.getElementById('bannerWorkout').style.borderColor = stage.col;
            
            targetSPM = stage.spm; 
            targetPeak = stage.peak;
            targetDuty = stage.duty;
            updateTargetLabels();
            
            let m = Math.floor(itTimeLeft / 60);
            let s = itTimeLeft % 60;
            document.getElementById('wTimer').innerText = `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`;
            
            itTimeLeft--;
            globalTimeElapsed++;
        }

        function updateTargetLabels() {
            document.getElementById('tgtSpm').innerText = targetSPM > 0 ? `Target: ${targetSPM} (${targetDuty}% tirata)` : "";
            document.getElementById('tgtPeak').innerText = targetPeak > 0 ? `Target: ${targetPeak} Kg` : "";
        }

        function draw() {
            canvas.width = canvas.clientWidth;
            canvas.height = canvas.clientHeight;
            ctx.clearRect(0,0, canvas.width, canvas.height);
            
            let chartBottom = canvas.height;
            let barH = 26;
            if ((mode === 'interval' || mode === 'hiit_adv' || mode === 'tabata' || mode === 'pyramid') && itRunning && itStages.length > 0) {
                chartBottom -= barH;
            }

            let maxV = Math.max(...history, targetPeak * 1.2, 10);
            let sY = chartBottom / maxV;
            let POINTS = 60; 
            let sX = canvas.width / (POINTS - 1);
            
            if(targetPeak > 0 && targetSPM > 0) {
                let framesPerStroke = (60.0 / targetSPM) * 25.0;
                let dutyPerc = targetDuty / 100.0;
                
                let pullFrames = framesPerStroke * dutyPerc;
                let recoveryFrames = framesPerStroke - pullFrames;
                let w = pullFrames / 2.0; 
                let peakPos = w; 
                
                ctx.beginPath();
                for(let i=0; i<POINTS; i++) {
                    let cyclePos = (i + frameCounter) % framesPerStroke;
                    let diff = cyclePos - peakPos;
                    
                    let bell = 0;
                    if(Math.abs(diff) <= w && w > 0) {
                        bell = 1.0 - Math.pow(diff / w, 4);
                    }
                    
                    let y = chartBottom - (targetPeak * bell * sY);
                    if(i==0) ctx.moveTo(0, y); else ctx.lineTo(i * sX, y);
                }
                ctx.strokeStyle = "rgba(217, 70, 239, 1.0)";
                ctx.lineWidth = 4; 
                ctx.setLineDash([10, 5]); 
                ctx.stroke(); 
                ctx.setLineDash([]);
                
                let trackW = canvas.width / 3.0;
                let trackX = 35;
                let trackY = 55;
                
                ctx.beginPath();
                ctx.moveTo(trackX, trackY - 10);
                ctx.lineTo(trackX, trackY + 10);
                ctx.moveTo(trackX + trackW, trackY - 10);
                ctx.lineTo(trackX + trackW, trackY + 10);
                ctx.strokeStyle = "rgba(255,255,255,0.8)";
                ctx.lineWidth = 3;
                ctx.stroke();

                ctx.beginPath();
                ctx.moveTo(trackX, trackY);
                ctx.lineTo(trackX + trackW, trackY);
                ctx.strokeStyle = "rgba(255,255,255,0.2)";
                ctx.lineWidth = 4;
                ctx.stroke();
                
                let presentCyclePos = (POINTS - 1 + frameCounter) % framesPerStroke;
                let isPull = presentCyclePos <= pullFrames;
                let seatX = 0;
                if (isPull) {
                    let p = presentCyclePos / pullFrames;
                    seatX = trackX + (p * trackW); 
                } else {
                    let p = (presentCyclePos - pullFrames) / recoveryFrames;
                    seatX = (trackX + trackW) - (p * trackW);
                }
                
                ctx.fillStyle = "#ffffff";
                ctx.beginPath();
                ctx.arc(seatX, trackY, 7, 0, Math.PI * 2);
                ctx.fill();
                
                let phaseColor = isPull ? "#ef4444" : "#10b981";
                ctx.fillStyle = phaseColor;
                ctx.fillRect(seatX - 15, trackY + 12, 30, 6);
                
                ctx.font = "bold 11px sans-serif";
                ctx.textAlign = "center";
                ctx.textBaseline = "alphabetic";
                ctx.fillText(isPull ? "TIRATA" : "RILASCIO", seatX, trackY - 12);
                
                ctx.beginPath(); ctx.setLineDash([2,4]);
                let peakY = chartBottom - (targetPeak * sY);
                ctx.moveTo(0, peakY); ctx.lineTo(canvas.width, peakY);
                ctx.strokeStyle = "rgba(255,255,255,0.2)"; ctx.stroke(); ctx.setLineDash([]);
            }

            if(history.length >= 2) {
                ctx.beginPath();
                ctx.moveTo(0, chartBottom);
                for(let i=0; i<history.length; i++) ctx.lineTo(i*sX, chartBottom - (history[i]*sY));
                ctx.lineTo((history.length-1)*sX, chartBottom);
                let g = ctx.createLinearGradient(0,0,0,chartBottom);
                g.addColorStop(0, 'rgba(34, 211, 238, 0.4)'); g.addColorStop(1, 'transparent');
                ctx.fillStyle = g; ctx.fill();

                ctx.beginPath();
                for(let i=0; i<history.length; i++) {
                    let y = chartBottom - (history[i]*sY);
                    if(i==0) ctx.moveTo(0, y); else ctx.lineTo(i*sX, y);
                }
                ctx.strokeStyle = "#22d3ee";
                ctx.lineWidth = 3; ctx.lineJoin = 'round'; ctx.stroke();
            }

            if ((mode === 'interval' || mode === 'hiit_adv' || mode === 'tabata' || mode === 'pyramid') && itRunning && itStages.length > 0) {
                let stage = itStages[itStageIndex];
                let progress = globalTimeElapsed / globalTotalTime;
                if(progress > 1) progress = 1;

                let barY = canvas.height - barH;
                ctx.fillStyle = "#1e293b";
                ctx.fillRect(0, barY, canvas.width, barH);

                ctx.fillStyle = stage.col;
                ctx.fillRect(0, barY, canvas.width * progress, barH);

                ctx.fillStyle = "white";
                ctx.font = "bold 13px sans-serif";
                ctx.textBaseline = "middle";

                let elMin = Math.floor(globalTimeElapsed/60); let elSec = globalTimeElapsed%60;
                let remTime = globalTotalTime - globalTimeElapsed;
                if(remTime < 0) remTime = 0;
                let remMin = Math.floor(remTime/60); let remSec = remTime%60;
                
                ctx.textAlign = "left";
                ctx.fillText(`  ${elMin}:${elSec.toString().padStart(2,'0')}`, 0, barY + barH/2);

                ctx.textAlign = "right";
                ctx.fillText(`-${remMin}:${remSec.toString().padStart(2,'0')}  `, canvas.width, barY + barH/2);
            }

            if ((mode === 'interval' || mode === 'hiit_adv' || mode === 'tabata' || mode === 'pyramid') && itRunning && itStages.length > 0) {
                ctx.textAlign = "left";
                ctx.textBaseline = "top";
                ctx.font = "bold 14px sans-serif";
                
                let timeStr = document.getElementById('wTimer').innerText;
                if (itStageIndex < itStages.length - 1) {
                    let nextStage = itStages[itStageIndex + 1];
                    ctx.fillStyle = nextStage.col; 
                    ctx.fillText(`Prossima: ${nextStage.name} in ${timeStr}`, 10, 10);
                } else {
                    ctx.fillStyle = "#10b981";
                    ctx.fillText(`Fine Allenamento in ${timeStr}`, 10, 10);
                }
            }
        }

        function processPhysical(f) {
            // Uso le soglie dinamiche anziché valori fissi
            if(f > pullThresh) { 
                if(!isPulling) isPulling = true;
                if(f > currentPeak) { 
                    currentPeak = f;
                    document.getElementById('fPeak').innerHTML = `${f.toFixed(1)}<span class="unit">kg</span>`;
                }
            } else if (f < relThresh && isPulling) { 
                isPulling = false;
                totalStrokes++; let now = Date.now();
                if(lastStrokeTime > 0) {
                    let durationSecs = (now - lastStrokeTime) / 1000.0;
                    let spm = Math.round(60 / durationSecs);
                    if (spm < 80) document.getElementById('fSpm').innerText = spm;

                    let heightCm = parseFloat(document.getElementById('uHeight').value) || 181.0;
                    let strokeLengthMeters = heightCm * 0.007; 
                    let avgForceNewtons = (currentPeak * 0.6) * 9.81; 
                    let workJoules = avgForceNewtons * strokeLengthMeters;
                    
                    let strokeWatts = workJoules / durationSecs;
                    sumWatts += strokeWatts;
                    
                    if(strokeWatts > maxStrokeWatts) maxStrokeWatts = strokeWatts;

                    let speedMs = 0;
                    if (strokeWatts > 0) speedMs = Math.pow(strokeWatts / 2.8, 1/3); 
                    totalDistance += speedMs * durationSecs;
                    
                    if (speedMs > 0) {
                        let paceSeconds = 500.0 / speedMs;
                        let pMin = Math.floor(paceSeconds / 60);
                        let pSec = Math.floor(paceSeconds % 60);
                        document.getElementById('sPace').innerText = `${pMin}:${pSec.toString().padStart(2,'0')}`;
                    }
                    
                    totalKcal += ((workJoules / 4184.0) * 4.0);
                    
                    document.getElementById('sWatts').innerHTML = `${Math.round(strokeWatts)}<span class="unit">W</span>`;
                    document.getElementById('sKcal').innerHTML = `${totalKcal.toFixed(1)}<span class="unit">kcal</span>`;
                    document.getElementById('sDist').innerHTML = `${Math.round(totalDistance)}<span class="unit">m</span>`;
                }
                lastStrokeTime = now;
                document.getElementById('sTotal').innerText = totalStrokes; 
                currentPeak = 0;
            }
        }

        function sendCommand(c) { if(websocket && websocket.readyState===1) websocket.send(c); }
        
        function saveProfile() {
            let h = document.getElementById('uHeight').value;
            let w = document.getElementById('uWeight').value;
            let p = document.getElementById('uPull').value;
            let r = document.getElementById('uRel').value;
            pullThresh = parseFloat(p);
            relThresh = parseFloat(r);
            sendCommand(`profile:${h}|${w}|${p}|${r}`);
            let msg = document.getElementById('profMsg'); msg.innerText = "Profilo e soglie salvati!";
            setTimeout(() => msg.innerText="", 4000);
        }
        
        function calibrateScale() {
            let w = document.getElementById('calibWeight').value;
            if(w > 0) {
                sendCommand("calib:" + w);
                let msg = document.getElementById('calibMsg');
                msg.innerText = "Calibrazione salvata in memoria persistente!"; setTimeout(() => msg.innerText="", 4000);
            }
        }

        function initWS() {
            websocket = new WebSocket(gateway);
            websocket.onmessage = (e) => { 
                if(e.data.startsWith("B:")) { 
                    let vals = e.data.substring(2).split(",");
                    let maxBatchForce = 0;
                    let lastVal = 0;
                    
                    for(let v of vals) {
                        let f = parseFloat(v);
                        if (f < 0) f = 0;
                        lastVal = f;
                        if (f > maxBatchForce) maxBatchForce = f;
                        processPhysical(f);
                    }
                    
                    document.getElementById('fLive').innerHTML = `${lastVal.toFixed(1)}<span class="unit">kg</span>`;
                    history.push(maxBatchForce); 
                    if(history.length > 60) history.shift(); 
                    frameCounter++; 
                    
                    if(!drawPending) {
                        drawPending = true;
                        requestAnimationFrame(() => {
                            draw();
                            drawPending = false;
                        });
                    }
                } 
                else if(e.data.startsWith("CONFIG:")) {
                    let parts = e.data.substring(7).split("|");
                    if(parts.length >= 4) { 
                        document.getElementById('uHeight').value = parts[0];
                        document.getElementById('uWeight').value = parts[1];
                        document.getElementById('uPull').value = parts[2];
                        document.getElementById('uRel').value = parts[3];
                        pullThresh = parseFloat(parts[2]);
                        relThresh = parseFloat(parts[3]);
                    }
                }
            };
            websocket.onclose = () => setTimeout(initWS, 2000);
        }
        
        window.onload = () => { 
            initStorage();
            updateWorkoutUI();
            initWS(); 
        };
    </script>
</body>
</html>
)rawliteral";

// =================================================================================
// GESTIONE WEBSOCKET
// =================================================================================
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        String cmd = String((char*)data);
        cmd.trim(); 

        if (cmd == "tare") {
            valoreTara = ads.getRawData();
            prefs.putInt("tare", valoreTara); 
        }
        else if (cmd.startsWith("calib:")) {
            fattoreDiScala = (float)(ads.getRawData() - valoreTara) / cmd.substring(6).toFloat();
            prefs.putFloat("scale", fattoreDiScala); 
        }
        else if (cmd.startsWith("profile:")) {
            int sep1 = cmd.indexOf('|');
            int sep2 = cmd.indexOf('|', sep1 + 1);
            int sep3 = cmd.indexOf('|', sep2 + 1);
            
            if (sep1 > 0 && sep2 > 0 && sep3 > 0) {
                userHeight = cmd.substring(8, sep1).toFloat();
                userWeight = cmd.substring(sep1 + 1, sep2).toFloat();
                pullThreshold = cmd.substring(sep2 + 1, sep3).toFloat();
                releaseThreshold = cmd.substring(sep3 + 1).toFloat();
                
                prefs.putFloat("uHeight", userHeight);
                prefs.putFloat("uWeight", userWeight);
                prefs.putFloat("uPull", pullThreshold);
                prefs.putFloat("uRel", releaseThreshold);
            }
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) handleWebSocketMessage(arg, data, len);
    if (type == WS_EVT_CONNECT) {
        client->text("CONFIG:" + String(userHeight, 1) + "|" + String(userWeight, 1) + "|" + String(pullThreshold, 1) + "|" + String(releaseThreshold, 1));
    }
}

// =================================================================================
// SETUP ESP32
// =================================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    prefs.begin("rower", false);
    valoreTara = prefs.getInt("tare", 0);
    fattoreDiScala = prefs.getFloat("scale", 10000.0);
    userHeight = prefs.getFloat("uHeight", 181.0);
    userWeight = prefs.getFloat("uWeight", 75.0);
    pullThreshold = prefs.getFloat("uPull", 3.0);
    releaseThreshold = prefs.getFloat("uRel", 1.5);

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    ads.init();
    ads.setCompareChannels(ADS1220_MUX_1_2);        
    ads.setVRefSource(ADS1220_VREF_AVDD_AVSS);      
    ads.setGain(ADS1220_GAIN_128);  
    ads.setDataRate(ADS1220_DR_LVL_6);
    ads.setConversionMode(ADS1220_CONTINUOUS);      

    WiFi.mode(WIFI_AP); 
    WiFi.softAP(ssid, password);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
        request->send_P(200, "text/html", (const uint8_t*)index_html, sizeof(index_html) - 1); 
    });
    
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = "<!DOCTYPE html><html><head><title>OTA Update</title><style>body{background:#020617; color:#fff; text-align:center; font-family:sans-serif; padding:50px;}</style></head><body><h2>Aggiornamento Firmware</h2><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update' accept='.bin' style='padding:20px;'><br><input type='submit' value='AVVIA FLASH' style='padding:10px 20px; background:#22d3ee; color:#000; font-weight:bold; border:none; cursor:pointer; border-radius:5px;'></form><br><br><button onclick=\"window.location.href='/'\" style='padding:10px; background:#334155; color:white; border:none; border-radius:5px; cursor:pointer;'>Torna alla Dashboard</button></body></html>";
        request->send(200, "text/html", html);
    });
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        bool shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK: Riavvio in corso..." : "ERRORE FLASH");
        response->addHeader("Connection", "close");
        request->send(response);
        if(shouldReboot) { delay(500); ESP.restart(); }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if(!index) Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
        if(!Update.hasError()) Update.write(data, len);
        if(final) Update.end(true);
    });

    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.begin();
    ArduinoOTA.setHostname("rowing-tracker");
    ArduinoOTA.begin();

    // --- SETUP BLUETOOTH EXR/KINOMAP ---
    BLEDevice::init("Smart Rower Pro");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pFtmsService = pServer->createService(FTMS_SERVICE_UUID);
    BLECharacteristic *pFtmsFeature = pFtmsService->createCharacteristic(FTMS_FEATURE_UUID, BLECharacteristic::PROPERTY_READ);
    uint8_t ftmsFeatureData[8] = {0x26, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
    pFtmsFeature->setValue(ftmsFeatureData, 8);
    
    pFtmsRowerData = pFtmsService->createCharacteristic(ROWER_DATA_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pFtmsRowerData->addDescriptor(new BLE2902());
    BLECharacteristic *pFtmsControlPoint = pFtmsService->createCharacteristic(FTMS_CONTROL_POINT_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    pFtmsControlPoint->addDescriptor(new BLE2902());
    pFtmsControlPoint->setCallbacks(new FTMSControlPointCallbacks());

    pFtmsService->start();
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(FTMS_SERVICE_UUID); 
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
}

// =================================================================================
// LOOP ESP32 E TRASMISSIONE BLE CONTINUA A 1 Hz
// =================================================================================
void loop() {
    ArduinoOTA.handle();
    ws.cleanupClients();
    
    static unsigned long lastBleNotify = 0;
    
    // Variabili per l'acquisizione a 1kHz
    static unsigned long lastMicros = 0;
    static float batchBuffer[100]; // Portato a 100 letture
    static int batchIndex = 0;
    
    static bool espIsPulling = false;
    static float espCurrentPeak = 0;
    static unsigned long espLastStrokeTime = 0;
    static uint16_t cumulativeStrokes = 0;
    static float espTotalDistance = 0.0;
    static uint16_t currentWatts = 0;
    static uint16_t spm = 0;
    static uint16_t paceSeconds = 0;

    // --- LETTURA SENSORE a 1kHz ---
    unsigned long currentMicros = micros();
    if (currentMicros - lastMicros >= 1000) {
        lastMicros = currentMicros;
        forzaKg = ((float)ads.getRawData() - (float)valoreTara) / fattoreDiScala; 
        
        // Uso le nuove soglie impostabili dalla dashboard
        if (forzaKg > pullThreshold) {
            espIsPulling = true;
            if (forzaKg > espCurrentPeak) espCurrentPeak = forzaKg;
        } 
        else if (forzaKg < releaseThreshold && espIsPulling) {
            espIsPulling = false;
            unsigned long now = millis();
            
            if (espLastStrokeTime > 0) {
                float durationSecs = (now - espLastStrokeTime) / 1000.0;
                spm = (uint16_t)(60.0 / durationSecs);
                float strokeLengthMeters = userHeight * 0.007;
                float workJoules = (espCurrentPeak * 0.6 * 9.81) * strokeLengthMeters; 
                currentWatts = (uint16_t)(workJoules / durationSecs);
                
                float speedMs = 0;
                if (currentWatts > 0) speedMs = pow((float)currentWatts / 2.8, 1.0/3.0);
                espTotalDistance += (speedMs * durationSecs);
                if (speedMs > 0) paceSeconds = (uint16_t)(500.0 / speedMs);

                cumulativeStrokes++;
            }
            espLastStrokeTime = now;
            espCurrentPeak = 0;
        }
        
        batchBuffer[batchIndex++] = forzaKg;
        
        // Invia i dati ogni 100 millisecondi (10 Hz) invece di 40ms, per scaricare il Wi-Fi
        if (batchIndex >= 100) {
            if(ws.count() > 0) {
                String payload;
                payload.reserve(600); 
                payload = "B:";
                for(int i = 0; i < 100; i++) {
                    payload += String(batchBuffer[i], 1);
                    if (i < 99) payload += ",";
                }
                ws.textAll(payload);
            }
            batchIndex = 0;
        }
    }

    // --- TRASMISSIONE BLE CONTINUA (1 Hz) ---
    if (deviceConnected && (millis() - lastBleNotify >= 1000)) {
        lastBleNotify = millis();
        if (millis() - espLastStrokeTime > 4000) {
            currentWatts = 0;
            spm = 0;
            paceSeconds = 0;
        }

        uint16_t ftmsFlags = 0x002C;
        uint8_t strokeRate = (spm * 2) > 255 ? 255 : (uint8_t)(spm * 2); 
        uint32_t dist24 = (uint32_t)espTotalDistance;

        uint8_t ftmsPayload[12];
        ftmsPayload[0] = ftmsFlags & 0xFF;
        ftmsPayload[1] = (ftmsFlags >> 8) & 0xFF;
        ftmsPayload[2] = strokeRate;
        ftmsPayload[3] = cumulativeStrokes & 0xFF;
        ftmsPayload[4] = (cumulativeStrokes >> 8) & 0xFF;
        ftmsPayload[5] = dist24 & 0xFF;
        ftmsPayload[6] = (dist24 >> 8) & 0xFF;
        ftmsPayload[7] = (dist24 >> 16) & 0xFF;
        ftmsPayload[8] = paceSeconds & 0xFF;
        ftmsPayload[9] = (paceSeconds >> 8) & 0xFF;
        ftmsPayload[10] = currentWatts & 0xFF;
        ftmsPayload[11] = (currentWatts >> 8) & 0xFF;

        pFtmsRowerData->setValue(ftmsPayload, 12);
        pFtmsRowerData->notify();
    }
}
