# Revisione SmartRower 1.0 (2026-07-02)

Esito della revisione dei progetti `SmartRower_Telaio_S3`, `SmartRower_Maniglia_S3`
e `shared/` che ha guidato il refactoring 2.0.

## Problemi risolti nella 2.0

| # | Problema | Dove (1.0) | Fix in 2.0 |
|---|----------|-----------|------------|
| 1 | Logica del colpo duplicata: media SPM, clamp watt, pace Concept2 e kcal implementati due volte, con lievi divergenze | `Physics::processStrokeComplete` vs `shared/StrokeEngine.h` | Unica implementazione in `shared/StrokeMath.h`, usata da entrambi i percorsi |
| 2 | Costruzione `String` (~2 KB, allocazioni heap) dentro `portENTER_CRITICAL` → interrupt disabilitati durante malloc; rischio jitter sul task fisico a 1 kHz e latch-up | `GhostPhysics::getJSON` | Snapshot delle LUT sotto lock (3 memcpy), formattazione con `snprintf` su buffer statico fuori dal lock. Idem `updateCurves`: calcolo fuori, pubblicazione sotto lock |
| 3 | Task fisico a 1000 Hz, ma forza a 100 Hz e campionamento grafico a 100 Hz: ~80% dei cicli non produceva nulla | `Physics::taskLoop` | Loop a 200 Hz (5 ms); campionamento 10 ms e integrazione lavoro invariati (l'integrale forza×spostamento è telescopico sui delta encoder, nessuna perdita di precisione) |
| 4 | Messaggio CFG costruito 2 volte e inviato 2 volte allo stesso client (`c->text` + `ws.textAll`) | `WebUI.cpp` handler `CFG:` | Un solo `textAll` (raggiunge anche il mittente) |
| 5 | `html.h` da 272 KB orfano (mai incluso) e header gzip da 280 KB duplicati in due progetti, rigenerati a mano con `pack_html.ps1` | `src/html.h`, `src/WebUI_HTML.h` ×2 | `web/index.html` unica fonte; `tools/gen_webui.py` genera l'header a build time (extra_scripts PlatformIO) |
| 6 | Pacchetti comando ESP-NOW impacchettati a mano byte per byte in 4 funzioni | `Telemetry.cpp` | `CmdPacket` + `makeCmdPacket` in `RowerProtocol.h` (formato wire invariato, 6 byte) |
| 7 | `Serial.printf` su ogni errore TX ESP-NOW: a 100 Hz può floodare la seriale quando il telaio è spento | `Maniglia/main.cpp` | Rate-limit a 1 log/secondo (`EspNowLink::sendForce`) |
| 8 | `main.cpp` maniglia monolitico (ESP-NOW, selezione modalità, calibrazione, TX, standalone tutto insieme) | `Maniglia/main.cpp` | ESP-NOW estratto in `EspNowLink`, loop suddiviso in `selectMode` / `processRemoteCommand` / `loopTransmitter` / `loopStandalone` |
| 9 | `powf(x, 1/3)` per il pace ad ogni colpo | entrambi | `cbrtf` (più veloce e preciso) |
| 10 | Chiavi PB costruite con `String` (heap) ad ogni lettura | `ConfigManager::getPB/savePB` | `snprintf` su buffer stack |
| 11 | Parametro `peakForce` mai usato | `Physics::processStrokeComplete` | Rimosso |
| 12 | Sanitizzazione soglie copiata in 3 punti | `ConfigManager` | `sanitizeThresholds()` unica |

## Comportamenti preservati intenzionalmente (identici alla 1.0)

- Tutti i formati wire e UI: `ForcePacket` (14 byte), heartbeat, comandi (6 byte),
  messaggi `CFG:`/`METRICS:`/`PBS:`/`WIFI_LIST:`, buffer binario 39 float, JSON ghost.
- Chiavi NVS (`rower`, `rower_calib`): nessuna ricalibrazione necessaria dopo il flash.
- Sequenza di boot, selezione modalità maniglia (heartbeat 3 s / pulsante),
  pairing ESP-NOW sul primo pacchetto, timeout 500 ms, auto-zero cavo a 2 s.
- OTA (`rowing-tracker`/`rower-ota`) e pagina `/update`.

## Fix funzionali FASE 2 (richieste utente, 2026-07-02)

| # | Problema segnalato | Fix |
|---|--------------------|-----|
| A | OTA maniglia non funzionava (in 1.0 non esisteva alcun endpoint) | Pagina `/update` + ArduinoOTA (`rower-handle`/`rower-ota`) in modalità standalone |
| B | Update parametri fisici maniglia non funzionava | La UI invia le soglie con `mech:pull\|rel`, che lo standalone 1.0 ignorava → aggiunto handler in `WebUIStandalone` |
| C | Maniglia autonoma deve creare la rete `RP_AP` | SoftAP standalone rinominato RP_Handle → RP_AP (il marker RP_Handle resta nella CFG per la UI) |
| D | Il cavo "tornava indietro" in ritardo rispetto alla fine della tirata | In 1.0 la posizione usava `abs(delta)` e cambiava verso solo sotto relThresh. Ora delta CON segno + direzione auto-appresa dal movimento netto della tirata: il ritorno parte appena il tamburo si riavvolge |
| E | Watt/distanza/kcal anche con encoder rotto | Toggle "Metriche da Encoder" in Setup (`encmode:`, NVS `encEn`): OFF → stima dal picco forza. Contestualmente risolto il problema noto #1: il picco usato ora è il massimo dei `peakCenti` sull'intera tirata, non l'ultima finestra da 10 ms |
| F | Forza max colpo al posto della forza istantanea | Card "Forza Max Colpo": picco live durante la tirata, mantenuto fino al colpo successivo |
| G | Modalità Coach con feedback sulle curve | Tab Coach client-side: modello Beta del super-prompt (v. sotto) |

## Compatibilità fisica col super-prompt (`super_prompt_antigravity_SmartRowerPro.md`)

Il documento si riferisce alla VECCHIA architettura (SmartRowerPro.ino monolitico,
BLE FTMS, webapp.h) ma il modello matematico è applicabile. Verifica:

- **Campionamento forza 100 Hz** ✓ (TX maniglia 10 ms, terzine buffer 10 ms — il
  documento conferma che i "1 kHz" del vecchio README erano in realtà 100 Hz)
- **Segmentazione a soglie kg** ✓ (pullThresh/relThresh riusati come chiede §1)
- **Profilo/soglie in Preferences NVS** ✓
- **Tier B disponibile**: l'hardware attuale ha già encoder cavo + ToF sedile →
  work=∫F·dx ✓ (Physics.cpp), L_drive, RSF e decomposizione possibili ✓
- **Divergenza**: la curva target on-device (GhostPhysics) è un modello Kleshnev
  a segmenti corporei (F = b2·v² + Meff·a), NON la famiglia Beta di §3.1.
  Il coach 2.0 implementa la Beta φ(u;p,q) in JS (LUT 64 punti, preset
  technique p=2,q=3 / race p=2,q=2.5, normalizzazione numerica al posto di lgamma)
  senza toccare il Ghost Pacer esistente.
- **Non applicabile**: BLE FTMS (assente nell'architettura attuale — c'è il client
  BLE per la fascia HR), analizzatore CSV Python host (non richiesto; l'export CSV
  della UI già produce i dati necessari), buzzer.
- **RFD a regressione** ✓, **d = clamp(0.33+0.0075·(R−20))** ✓, **cue IT di §6.3** ✓,
  scoring §6.1 con tolleranze leggermente rilassate (Φ ±0.08, picco ±10pp, RFD ±40%)
  per essere usabili al primo utilizzo — restano costanti configurabili nel JS.
- **Catch factor**: non implementato (richiede interpolazione sub-campione, §3.4 lo
  definisce "metrica grossolana" a 100 Hz) — candidato per un'iterazione futura.

## Problemi noti NON toccati (decisioni da prendere a parte)

1. **Canale ESP-NOW fisso 6 con STA attiva**: se il router di casa non è sul canale 6,
   la maniglia non comunica (limite già documentato nel codice 1.0).
2. **Credenziali di default deboli**: SoftAP `password`, OTA `rower-ota` in chiaro
   nel sorgente. Accettabile per rete locale dedicata; da cambiare se esposto.
3. **Race teorica su `pendingCmd`/`pendingParam`** (callback ESP-NOW vs loop):
   finestra di pochi µs con comandi manuali dalla UI, impatto trascurabile.
   L'ordine di scrittura (param prima di cmd) in 2.0 la riduce ulteriormente.
4. **`telemetry.seatPositionMeters` letto senza lock** nel riempimento buffer:
   lettura float 32-bit atomica su Xtensa, nessun tearing possibile.
5. **Direzione encoder appresa a runtime**: se il primo colpo dopo il boot ha il
   segno "sbagliato", la posizione cavo resta a 0 per quella tirata e si corregge
   dal colpo successivo. Persistere il segno in NVS se dovesse dare fastidio.

## Verifica

- `pio run` Telaio: SUCCESS — RAM 18.8%, Flash 84.6%
- `pio run` Maniglia: SUCCESS — RAM 15.2%, Flash 66.0%
- Da testare su hardware: pairing ESP-NOW, OTA maniglia (`/update` su 192.168.4.1
  in standalone), soglie da UI standalone, ritorno cavo a fine tirata,
  toggle encoder OFF→metriche stimate, tab Coach dopo qualche colpo.
