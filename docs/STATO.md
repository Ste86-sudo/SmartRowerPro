# SmartRower 2.0 — Piano di refactoring e stato avanzamento

Questo file traccia lo stato del refactoring. Se il lavoro si interrompe,
riprendere dal primo step non marcato `[x]`.

Sorgente originale: `C:\Users\stefa\OneDrive\Desktop\SmartRower`
(progetti `SmartRower_Telaio_S3`, `SmartRower_Maniglia_S3`, `shared/`, `index.txt`).

**Vincolo fondamentale**: nessuna modifica ai formati wire/UI —
pacchetti ESP-NOW (`ForcePacket` 14 byte, heartbeat, comandi 6 byte),
messaggi WebSocket (`CFG:`, `METRICS:`, `PBS:`, `WIFI_LIST:`, buffer binario 39 float),
chiavi NVS (`rower`, `rower_calib`). La web UI (`web/index.html`) è riusata invariata.

## Step

- [x] **Step 0 — Revisione codice 1.0** (vedi `REVISIONE.md`)
- [x] **Step 1 — Struttura cartelle** (`shared/`, `web/`, `tools/`, `Telaio/`, `Maniglia/`) + copia `index.txt` → `web/index.html`
- [x] **Step 2 — Codice condiviso** (`shared/`)
  - `RowerProtocol.h`: formati pacchetto invariati + struct `CmdPacket` (sostituisce packing manuale)
  - `StrokeMath.h`: matematica del colpo unificata (SPM smoothing, watt, pace, kcal) — elimina la duplicazione Physics/StrokeEngine
  - `StrokeEngine.h`: macchina a stati pull/release basata su StrokeMath
- [x] **Step 3 — Tooling build** (`tools/gen_webui.py`)
  - Script SCons `pre:` che gzippa `web/index.html` → `include/WebUI_HTML.h` a build time
    (elimina lo step manuale `pack_html.ps1` e gli header da 280KB duplicati in git)
- [x] **Step 4 — Firmware Telaio** (`Telaio/`)
  - `platformio.ini` (stesse lib, + extra_scripts)
  - `Config.h`, `SharedData.{h,cpp}`, `ConfigManager.{h,cpp}`, `Encoder.{h,cpp}` (invariati nella sostanza)
  - `GhostPhysics.{h,cpp}`: JSON costruito FUORI dalla sezione critica, buffer statico snprintf (no String/heap)
  - `Physics.{h,cpp}`: usa StrokeMath condiviso; loop 1000 Hz → 200 Hz (campionamento 10 ms invariato)
  - `Telemetry.{h,cpp}`: usa CmdPacket
  - `HRMonitor.{h,cpp}`, `LaserSensor.{h,cpp}`: pulizia, logica invariata
  - `WebUI.{h,cpp}`: stessi comandi/formati; CFG inviata una volta sola; ghost JSON senza String
  - `main.cpp`: NetTask come funzione nominata, stessa sequenza di boot
- [x] **Step 5 — Firmware Maniglia** (`Maniglia/`)
  - `platformio.ini` (stesse lib, + extra_scripts)
  - `Config.h`, `CalibStore.{h,cpp}`, `LoadCell.{h,cpp}` (invariati nella sostanza)
  - `EspNowLink.{h,cpp}`: ESP-NOW estratto da main.cpp (pairing, heartbeat, comandi)
  - `PhysicsLite.{h,cpp}`: usa StrokeEngine condiviso
  - `WebUIStandalone.{h,cpp}`: stessi comandi/formati
  - `main.cpp`: solo orchestrazione (selezione modalità, loop TX/standalone)
- [x] **Step 6 — Compilazione**
  - `pio run` in `Telaio/` → SUCCESS (RAM 18.8%, Flash 84.1%)
  - `pio run` in `Maniglia/` → SUCCESS (RAM 13.6%, Flash 61.9%)
- [x] **Step 7 — Report finale** (`REVISIONE.md` completo + note di migrazione in `README.md`)

## FASE 2 — Fix funzionali richiesti (2026-07-02)

- [x] **Step 8 — Maniglia: OTA funzionante**
  - Pagina `/update` (upload .bin dal browser) + ArduinoOTA (`rower-handle`/`rower-ota`) in standalone
- [x] **Step 9 — Maniglia: update parametri fisici**
  - Handler `mech:` (soglie pull/rel dalla UI) — in 1.0 era ignorato
- [x] **Step 10 — Maniglia standalone su rete `RP_AP`**
  - SoftAP rinominato da RP_Handle a RP_AP (stesso SSID del telaio, unica rete sul telefono);
    il marker "RP_Handle" resta nel messaggio CFG per il rilevamento standalone della UI
- [x] **Step 11 — Telaio: fix ritorno cavo encoder**
  - Delta encoder CON segno + direzione auto-appresa durante la tirata:
    il cavo torna indietro appena il tamburo si riavvolge (prima aspettava relThresh)
- [x] **Step 12 — Telaio: toggle encoder (metriche senza encoder)**
  - `config.encoderEnabled` (NVS `encEn`), comando ws `encmode:0/1`,
    CFG esteso con 4ª sezione `|<enc>`; con toggle OFF watt/distanza/kcal
    stimati dal picco forza (fallback), con picco VERO del colpo
    (max dei peakCenti lungo tutta la tirata, non l'ultima finestra da 10 ms)
- [x] **Step 13 — UI: Forza Max Colpo**
  - La card in basso mostra il picco della tirata corrente/ultima (non più la forza istantanea)
- [x] **Step 14 — UI: modalità COACH** (tab dedicato)
  - Modello Beta del super-prompt (`super_prompt_antigravity_SmartRowerPro.md`):
    LUT target 64 punti, preset Technique/Race/Auto, feature per colpo
    (Φ, picco %, RFD a regressione, picchi secondari, RMSE forma, frazione drive;
    Tier B: L_drive, lavoro ∫F·dx, RSF), scoring a tolleranze, cue prioritario.
    Tutto lato client (JS): zero costo firmware.
- [x] **Step 15 — Ricompilazione**
  - Telaio: SUCCESS (RAM 18.8%, Flash 84.6%) — Maniglia: SUCCESS (RAM 15.2%, Flash 66.0%)

## FASE 3 — Scienza + coinvolgimento (2026-07-02)

- [x] **Step 16 — Auto-diagnosi encoder (firmware telaio)**
  - Tirata valida >600 ms con <20 tick → strike; 2 strike → fault, passaggio
    automatico alla stima da picco; rientro se l'encoder si muove di nuovo.
    Campo 15 in METRICS; banner rosso in dashboard; il Coach degrada a Tier A.
- [x] **Step 17 — Coach: P(t) = F·v istantanea** (curva viola sul canvas Coach,
  P̄ e Pmax del drive nel dettaglio colpo)
- [x] **Step 18 — Coach: catch factor** (pre-buffer 400 ms prima del catch,
  zero-crossing velocità sedile/cavo con interpolazione sub-campione,
  banda target [-35,-15] ms, cue "attacco statico"/"sedile scappa")
- [x] **Step 19 — Coach: fatica & costanza** (deriva RFD %/colpo, CV passata,
  SD posizione picco, decoupling Pw:HR)
- [x] **Step 20 — Parametri atleta** (card in Setup: età/sesso/HRrest/HRmax/CP/W′,
  solo localStorage — nessuna modifica NVS/protocollo)
- [x] **Step 21 — W′ balance** (modello Froncioni-Skiba-Clarke, card in dashboard,
  colore status sotto 50%/20%)
- [x] **Step 22 — Zone HR + TRIMP + energetica** (barra tempo-in-zona 5 zone
  sequenziali in dashboard; TRIMP Banister; kcal Concept2 e Keytel-da-HR
  affiancate a quelle firmware)
- [x] **Step 23 — Tab Storico** (sessioni in localStorage, riepilogo 7 giorni,
  tabella, trend distanza/potenza su canvas con tooltip, tabella PB)
- [x] **Step 24 — Ghost replay** ("SFIDA" su una sessione salvata: distanza ghost
  integrata dai watt registrati, chip ±metri in dashboard; tracce in IndexedDB, ultime 5)
- [x] **Step 25 — Analizzatore Python** (`tools/analyzer/analyze.py`: stesse
  feature/scoring del coach + fPCA dei modi personali; validato con sessione
  sintetica da Beta(2,3) — recupera picco/Φ/RSF iniettati)
- [x] **Step 26 — Ricompilazione**
  - Telaio: SUCCESS (RAM 18.8%, Flash 85.4%) — Maniglia: SUCCESS (RAM 15.2%, Flash 66.8%)

**Esclusi su richiesta**: cue vocali (8) e BLE FTMS (12).

## COMPLETATO — 2026-07-02

Prossimo passo (manuale): test su hardware — pairing ESP-NOW, OTA maniglia,
soglie da UI standalone, ritorno cavo, toggle encoder + auto-diagnosi
(scollegare l'encoder e verificare banner + metriche stimate), tab Coach
(catch factor con ToF attivo), salvataggio sessione + SFIDA ghost.

## Note per la ripresa

- I due progetti PlatformIO sono autonomi: aprire `Telaio/` o `Maniglia/` come root PIO.
- `include/WebUI_HTML.h` è GENERATO da `tools/gen_webui.py` al primo build — non va editato né versionato.
- Porte upload originali: Telaio COM18, Maniglia COM17.
- OTA telaio: hostname `rowing-tracker`, password `rower-ota` (invariati).
