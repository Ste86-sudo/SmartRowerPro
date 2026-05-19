#pragma once

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
            <div class="tab-btn" onclick="switchTab('workout')">WORKOUT</div>
            <div class="tab-btn" onclick="switchTab('calib')">SETUP & PROFILE</div>
        </div>

        <div id="tab-dashboard" class="tab-content active">
            <div class="workout-banner" id="bannerWorkout" style="display:none;">
                <div class="phase-text" id="wPhase" style="color:var(--accent);">WARMUP</div>
                <div style="font-size:3rem; font-weight:900; font-family:monospace;" id="wTimer">00:00</div>
            </div>

            <div class="grid-3">
                <div class="card" style="padding:15px;"><div class="label">Live Force</div><div class="val" id="fLive">0.0<span class="unit">kg</span></div></div>
                <div class="card" style="padding:15px;"><div class="label">Stroke Rate</div><div class="val" id="fSpm">0</div><div id="tgtSpm" class="label" style="color:var(--accent)"></div></div>
                <div class="card" style="padding:15px;"><div class="label">Stroke Peak</div><div class="val" id="fPeak">0.0<span class="unit">kg</span></div><div id="tgtPeak" class="label" style="color:var(--accent)"></div></div>
            </div>

            <div class="chart-box">
                <canvas id="rowCanvas"></canvas>
            </div>

            <div class="grid-3" style="margin-top:15px;">
                <div class="card" style="padding:15px;"><div class="label">Stroke Power</div><div class="val" id="sWatts" style="color:var(--power)">0<span class="unit">W</span></div></div>
                <div class="card" style="padding:15px;"><div class="label">Pace /500m</div><div class="val" id="sPace" style="color:var(--dist)">--:--</div></div>
                <div class="card" style="padding:15px;"><div class="label">Distance</div><div class="val" id="sDist" style="color:var(--dist)">0<span class="unit">m</span></div></div>
            </div>

            <div class="grid-2">
                <div class="card" style="padding:15px;"><div class="label">Total Strokes</div><div class="val" id="sTotal">0</div></div>
                <div class="card" style="padding:15px;"><div class="label">Calories</div><div class="val" id="sKcal" style="color:#fb7185">0.0<span class="unit">kcal</span></div></div>
            </div>
        </div>

        <div id="tab-workout" class="tab-content">
            <div class="card">
                <h3 style="margin-top:0;">Choose Workout Type</h3>
                <select id="wType" onchange="updateWorkoutUI()">
                    <option value="free">Free Style (No Target)</option>
                    <option value="target">Fixed Target (Find the perfect stroke)</option>
                    <option value="interval">Interval Training (Basic HIIT)</option>
                    <option value="hiit_adv">HIIT Advanced (Custom Multi-Set)</option>
                    <option value="tabata">Custom Tabata</option>
                    <option value="pyramid">Custom Pyramid</option>
                </select>

                <div id="ui-target" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-3">
                        <div><div class="label">Target SPM</div><input type="number" id="tSpm" value="24" oninput="liveUpdateTargets()"></div>
                        <div><div class="label">Target Force (Kg)</div><input type="number" id="tForce" value="30" oninput="liveUpdateTargets()"></div>
                        <div><div class="label">Pull Duty Cycle (%)</div><input type="number" id="tDuty" value="35" oninput="liveUpdateTargets()"></div>
                    </div>
                </div>

                <div id="ui-interval" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-4">
                        <div><div class="label" style="color:var(--accent)">Warmup (sec)</div><input type="number" id="iWarm" value="60"></div>
                        <div><div class="label" style="color:var(--accent)">Repetitions</div><input type="number" id="iReps" value="8"></div>
                        <div><div class="label" style="color:var(--accent)">Pull Duty (%)</div><input type="number" id="iWarmDuty" value="30"></div>
                        <div></div>

                        <div><div class="label" style="color:var(--work)">Sprint (sec)</div><input type="number" id="iWork" value="30"></div>
                        <div><div class="label" style="color:var(--work)">Target SPM</div><input type="number" id="iWorkSpm" value="28"></div>
                        <div><div class="label" style="color:var(--work)">Target Kg</div><input type="number" id="iWorkKg" value="40"></div>
                        <div><div class="label" style="color:var(--work)">Pull Duty (%)</div><input type="number" id="iWorkDuty" value="40"></div>

                        <div><div class="label" style="color:var(--rest)">Recovery (sec)</div><input type="number" id="iRest" value="30"></div>
                        <div><div class="label" style="color:var(--rest)">Target SPM</div><input type="number" id="iRestSpm" value="16"></div>
                        <div><div class="label" style="color:var(--rest)">Target Kg</div><input type="number" id="iRestKg" value="15"></div>
                        <div><div class="label" style="color:var(--rest)">Pull Duty (%)</div><input type="number" id="iRestDuty" value="25"></div>
                    </div>
                </div>

                <div id="ui-hiit-adv" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-4">
                        <div><div class="label" style="color:var(--accent)">Warmup (sec)</div><input type="number" id="haWarm" value="180"></div>
                        <div><div class="label" style="color:#3b82f6">Number of Sets</div><input type="number" id="haSets" value="3"></div>
                        <div><div class="label" style="color:var(--accent)">Reps per Set</div><input type="number" id="haReps" value="5"></div>
                        <div><div class="label" style="color:#3b82f6">Set Recovery (sec)</div><input type="number" id="haSetRest" value="120"></div>

                        <div><div class="label" style="color:var(--work)">Sprint (sec)</div><input type="number" id="haWork" value="40"></div>
                        <div><div class="label" style="color:var(--work)">Target SPM</div><input type="number" id="haWorkSpm" value="28"></div>
                        <div><div class="label" style="color:var(--work)">Target Kg</div><input type="number" id="haWorkKg" value="45"></div>
                        <div></div>

                        <div><div class="label" style="color:var(--rest)">Short Recovery (sec)</div><input type="number" id="haRest" value="20"></div>
                        <div><div class="label" style="color:var(--rest)">Target SPM</div><input type="number" id="haRestSpm" value="16"></div>
                        <div><div class="label" style="color:var(--rest)">Target Kg</div><input type="number" id="haRestKg" value="15"></div>
                        <div></div>
                    </div>
                </div>

                <div id="ui-tabata" style="display:none; padding-top:10px; border-top:1px solid #334155;">
                    <div class="grid-4">
                        <div><div class="label" style="color:var(--accent)">Warmup (sec)</div><input type="number" id="tbWarm" value="180"></div>
                        <div><div class="label" style="color:var(--accent)">Repetitions</div><input type="number" id="tbReps" value="8"></div>
                        <div></div><div></div>

                        <div><div class="label" style="color:var(--work)">Sprint (sec)</div><input type="number" id="tbWork" value="20"></div>
                        <div><div class="label" style="color:var(--work)">Target SPM</div><input type="number" id="tbWorkSpm" value="32"></div>
                        <div><div class="label" style="color:var(--work)">Target Kg</div><input type="number" id="tbWorkKg" value="40"></div>
                        <div></div>

                        <div><div class="label" style="color:var(--rest)">Recovery (sec)</div><input type="number" id="tbRest" value="10"></div>
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
                        <div><div class="label" style="color:var(--rest)">Step Recovery (sec)</div><input type="number" id="pyRest" value="60"></div>
                        <div></div>

                        <div><div class="label" style="color:var(--rest)">Recovery SPM</div><input type="number" id="pyRestSpm" value="16"></div>
                        <div><div class="label" style="color:var(--rest)">Recovery Kg</div><input type="number" id="pyRestKg" value="15"></div>
                        <div></div><div></div>
                    </div>
                </div>

                <button class="btn btn-primary" onclick="startWorkout()" style="margin-top:15px;">START WORKOUT</button>
            </div>
            <button class="btn btn-danger" onclick="endWorkout()">END WORKOUT / VIEW STATS</button>
        </div>

        <div id="tab-calib" class="tab-content">
            <div class="card">
                <h3 style="margin-top:0; color:var(--accent);">User Profile & Stroke Sensitivity</h3>
                <p style="color:#94a3b8; font-size:0.9rem;">Set your physical data and force thresholds (Kg) for stroke detection.</p>
                <div class="grid-2">
                    <div><div class="label">Height (cm)</div><input type="number" id="uHeight" value="181"></div>
                    <div><div class="label">Weight (Kg)</div><input type="number" id="uWeight" value="75"></div>
                    <div><div class="label" style="color:var(--accent)">Pull Start Threshold (Kg)</div><input type="number" id="uPull" value="3.0" step="0.1"></div>
                    <div><div class="label" style="color:var(--accent)">Release Threshold (Kg)</div><input type="number" id="uRel" value="1.5" step="0.1"></div>
                </div>
                <button class="btn btn-outline" onclick="saveProfile()" style="border-color:var(--accent); color:var(--accent);">SAVE PROFILE & THRESHOLDS</button>
                <div id="profMsg" style="color:#10b981; font-weight:bold; text-align:center; height:20px; margin-top:5px;"></div>
            </div>

            <div class="card">
                <h3 style="margin-top:0; color:var(--accent);">Step 1: Zero (Tare)</h3>
                <button class="btn btn-outline" onclick="sendCommand('tare')">SET ZERO</button>
            </div>

            <div class="card">
                <h3 style="margin-top:0; color:var(--accent);">Step 2: Weight Calibration</h3>
                <div class="label" style="text-align:left;">Applied weight (Kg):</div>
                <input type="number" id="calibWeight" value="10.0" step="0.1">
                <button class="btn btn-primary" onclick="calibrateScale()">CALIBRATE SENSOR</button>
            </div>
            <div id="calibMsg" style="color:#10b981; font-weight:bold; text-align:center; height:20px;"></div>
        </div>
    </div>

    <div id="statsModal" class="modal">
        <div class="modal-content">
            <h2>WORKOUT COMPLETE</h2>
            <div class="stat-row"><span>Total Time</span><span id="mTime" class="stat-val">00:00</span></div>
            <div class="stat-row"><span>Distance</span><span id="mDist" class="stat-val">0 m</span></div>
            <div class="stat-row"><span>Total Strokes</span><span id="mStrokes" class="stat-val">0</span></div>
            <div class="stat-row"><span>Avg Pace (/500m)</span><span id="mPace" class="stat-val">--:--</span></div>
            <div class="stat-row"><span>Avg Power</span><span id="mAvgWatts" class="stat-val" style="color:var(--power)">0 W</span></div>
            <div class="stat-row"><span>Peak Power</span><span id="mMaxWatts" class="stat-val" style="color:var(--power)">0 W</span></div>
            <div class="stat-row"><span>Calories</span><span id="mKcal" class="stat-val" style="color:#fb7185">0.0 kcal</span></div>
            <button class="btn btn-primary" style="margin-top:25px;" onclick="closeStats()">CLOSE & BACK TO DASHBOARD</button>
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

        let pullThresh = 3.0;
        let relThresh = 1.5;

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
                    itStages.push({name: `MAX EFFORT ${i+1}/${reps}`, dur: work, spm: wSpm, peak: wKg, duty: iWorkDuty, col: '#ef4444'});
                    itStages.push({name: `ACTIVE RECOVERY ${i+1}/${reps}`, dur: rest, spm: rSpm, peak: rKg, duty: iRestDuty, col: '#10b981'});
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
                        itStages.push({name: `SET ${s} - SPRINT ${r}/${reps}`, dur: work, spm: workSpm, peak: workKg, duty: 40, col: '#ef4444'});
                        if (r < reps) {
                            itStages.push({name: `SET ${s} - SHORT RECOVERY ${r}/${reps}`, dur: rest, spm: restSpm, peak: restKg, duty: 25, col: '#10b981'});
                        }
                    }
                    if (s < sets) {
                        itStages.push({name: `LONG RECOVERY (SET ${s} → ${s+1})`, dur: setRest, spm: 16, peak: 10, duty: 25, col: '#3b82f6'});
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
                    itStages.push({name: `PYRAMID EFFORT (${steps[i]} min)`, dur: steps[i]*60, spm: wSpm, peak: wKg, duty: 35, col: '#f59e0b'});
                    if(i < steps.length - 1) {
                        itStages.push({name: 'RECOVERY', dur: rest, spm: rSpm, peak: rKg, duty: 25, col: '#10b981'});
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
            document.getElementById('tgtSpm').innerText = targetSPM > 0 ? `Target: ${targetSPM} (${targetDuty}% pull)` : "";
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
                ctx.fillText(isPull ? "PULL" : "RELEASE", seatX, trackY - 12);

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
                    ctx.fillText(`Next: ${nextStage.name} in ${timeStr}`, 10, 10);
                } else {
                    ctx.fillStyle = "#10b981";
                    ctx.fillText(`Workout ending in ${timeStr}`, 10, 10);
                }
            }
        }

        function processPhysical(f) {
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
            let msg = document.getElementById('profMsg'); msg.innerText = "Profile and thresholds saved!";
            setTimeout(() => msg.innerText="", 4000);
        }

        function calibrateScale() {
            let w = document.getElementById('calibWeight').value;
            if(w > 0) {
                sendCommand("calib:" + w);
                let msg = document.getElementById('calibMsg');
                msg.innerText = "Calibration saved to persistent storage!"; setTimeout(() => msg.innerText="", 4000);
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
