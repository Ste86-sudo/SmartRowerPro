# SUPER-PROMPT PER ANTIGRAVITY
## Coach virtuale integrato in SmartRowerPro (ESP32) + analizzatore CSV post-sessione

---

## 0. RUOLO E OBIETTIVO

Agisci come ingegnere senior embedded (Arduino/ESP32) + data engineer. Devi **estendere il progetto esistente `SmartRowerPro`** (https://github.com/Ste86-sudo/SmartRowerPro) aggiungendo un "coach tecnico" che valuta la qualità della vogata e suggerisce un cue, SENZA rompere le funzioni attuali (BLE FTMS verso EXR/Kinomap, Web App, programmi di allenamento, OTA).

Due deliverable che condividono lo stesso modello matematico:
1. **FIRMWARE (on-device, leggero):** segmentazione vogata, poche feature scalari, confronto con una curva target precalcolata, emissione di **un cue prioritario**; overlay della curva target sul plot esistente; storage dei parametri; logging/stream CSV per l'host.
2. **ANALIZZATORE CSV (host, Python, pesante):** rilegge i log, ricalcola tutte le feature, scoring, report di sessione, e l'hook fPCA/B-spline per la personalizzazione (Stadio 2).

**PRIMA di scrivere codice, leggi il repo e fammi queste domande se mancano risposte:**
(a) variante esatta della board (ESP32-S3 con/ senza PSRAM, o C3?) e budget RAM residuo con Wi-Fi+BLE+WebApp attivi;
(b) **per il Tier B**: modello e interfaccia dell'encoder manubrio (incrementale A/B? SPI? quanti CPR?) e del laser ToF seggiolino (es. VL53L0X/L1X su I2C?), e i GPIO liberi (attenzione: GPIO 0,1,2,3,5 sono già usati dall'ADS1220 in SPI);
(c) dove preferisci il feedback (solo Web App? anche un campo BLE? buzzer?);
(d) come vuoi i log CSV (stream live al browser e salvataggio lato client, oppure SD card?).

---

## 1. STATO ATTUALE DEL PROGETTO (da rispettare)

- **MCU:** ESP32-S3 / ESP32-C3, framework **Arduino** (sketch `SmartRowerPro.ino`), linguaggio C/C++.
- **Sensore forza:** cella di carico (S-Type/Ring) che **sostituisce meccanicamente il manubrio**, letta via **ADS1220 (24-bit ADC) su SPI**. ⚠️ **Frequenza di campionamento forza reale = 100 Hz** (il README dice "1 kHz" ma il rate effettivo è 100 Hz: usa 100 Hz ovunque).
- **Segmentazione vogata attuale:** soglie in Kg configurabili da Web App per inizio-tirata (drive start) e ritorno-manubrio (recovery). **Riusa questo meccanismo**, non reinventarlo.
- **Calcolo metrico attuale:** Watt, SPM, distanza, pace — derivati dalla forza. Profilo utente (Height, Weight) e soglie salvati in **`Preferences`** (NVS flash).
- **Connettività:** **BLE FTMS** (priorità hardware all'antenna BT — NON degradare questo path) + **Web App** servita dall'ESP32 (AP `RP_AP`, `http://192.168.4.1`) tramite **ESPAsyncWebServer/AsyncTCP**, con HTML/JS in **`webapp.h`** (PROGMEM).
- **Feature esistenti rilevanti:** "Ghost Pacer", plotting live della curva di forza, programmi (Free Style, Fixed Target, HIIT, Tabata, Pyramid), **OTA** via `/update`.
- **Librerie:** `AsyncTCP` 3.3.9, `ESPAsyncWebServer` 3.7.6, `ADS1220_WE` 1.0.6 + core ESP32 (`WiFi`,`SPI`,`Preferences`,`ArduinoOTA`,`Update`,`BLE*`).
- **File:** `SmartRowerPro.ino`, `webapp.h`, `sketch.yaml`, `INSTALL.md`, `README.md`.

**Vincoli embedded:** niente librerie matematiche pesanti on-device; niente allocazioni dinamiche nel loop; on-device usa solo `<math.h>` (incl. `lgamma` se serve). Tutto ciò che è B-spline/fPCA va SOLO sull'host.

---

## 2. DUE LIVELLI DI CAPACITÀ (decisione di progetto fondamentale)

Il firmware attuale ha **solo la forza**. Senza posizione del manubrio non esistono spostamento, lavoro ∫F dx, lunghezza passata, RSF, posizione del picco, catch factor. Quindi:

### TIER A — solo cella di carico (funziona sull'hardware attuale)
Metriche dal solo segnale di forza F(t) @100 Hz, sull'asse del **tempo normalizzato della passata** τ ∈ [0,1]:
- `F_peak`, `F_mean` (sul drive), **fullness** `Φ = F_mean/F_peak`
- **RFD** (vedi §3.4, regressione sul fronte di salita, non due punti)
- `t_to_peak`, posizione del picco in % del drive (in tempo), `impulse = ∫F dt`
- `drive_recovery_ratio` (da timing soglie Kg)
- **forma**: confronto della curva di forza normalizzata col target Beta in τ
- magnitudo: NON da potenza (manca L_drive). Usa come riferimento la **media mobile (EMA) del picco di forza** delle ultime N vogate del vogatore stesso → valuti la TECNICA (forma/timing), non quanto tira.

### TIER B — + encoder manubrio + ToF seggiolino (lavoro di integrazione)
Aggiunge i segnali `handle_pos` e `seat_pos` e sblocca lo spazio **posizione**:
- `work = ∫F dx`, `L_drive`, `power` reale, posizione del picco in % di spazio
- **RSF** (sedile:manubrio nel primo 20% del drive), decomposizione `upper_contrib = Δhandle − Δseat` (tronco+braccia)
- **catch_factor** (timing inversione sedile vs manubrio) — ⚠️ a 100 Hz = 10 ms/campione, vicino al limite: calcolalo con **interpolazione lineare sub-campione** dello zero-crossing della velocità e trattalo come metrica grossolana
- velocità `v_handle`, `v_seat`, gap a inizio drive → rilevamento "shooting the slide"
- magnitudo da potenza target: `F_mean = 60·P̄/(R·L_drive)` (vedi §3.3)

Implementa il coach con un **flag di compilazione/feature** (`#define COACH_TIER_B`) o rilevamento runtime dei sensori, così Tier A gira da subito e Tier B si attiva quando i sensori sono cablati. Per Tier B chiedi i dettagli hardware della §0(b).

---

## 3. MODELLO MATEMATICO CONDIVISO (`coach_model`)

Modulo unico (header C++ `coach_model.h` per il firmware + porting Python `coach_model.py` per l'host, verificati coerenti sui test). Tutte le costanti = parametri configurabili.

### 3.1 Forma target (famiglia Beta su frazione di passata u ∈ [0,1])
`u = τ` (tempo normalizzato, Tier A) oppure `u = (x−x_catch)/L_drive` (posizione, Tier B).
```
phi(u; p,q) = u^(p-1) * (1-u)^(q-1) / B(p,q)     // normalizzata: media = 1 su [0,1]
peak_pos u* = (p-1)/(p+q-2)
fullness  Φ = 1 / phi(u*)
```
Precalcola la curva target come **LUT** (64–128 punti). Su ESP32 puoi calcolare B(p,q) via `exp(lgamma(p)+lgamma(q)-lgamma(p+q))` una sola volta al cambio config; oppure ricevi la LUT già pronta dall'host. NON valutare phi() per ogni campione nel loop.

Preset:

| preset      | p   | q   | picco | Φ    | range SPM |
|-------------|-----|-----|-------|------|-----------|
| "technique" | 2.0 | 3.0 | 33%   | 0.56 | 18–26     |
| "race"      | 2.0 | 2.5 | 40%   | 0.61 | 30–36     |

Override manuale di (p,q) ammesso.

### 3.2 Geometria per atleta (solo Tier B)
```
L_drive_nominale = 0.78 * H     // H = altezza [m] (già nel profilo Preferences); banda 0.75–0.82*H
```
La geometria reale si fissa con la **calibrazione** (§5.1): `x_catch`, `x_finish`, `L_drive = x_finish − x_catch`. Allarme se `L_drive_misurata < 0.70*H`.

### 3.3 Magnitudo/timing
```
T        = 60 / R                                    // periodo colpo [s], R = SPM
// Tier B (con L_drive):
W_stroke = 60 * P̄ / R ;  F_mean = W_stroke / L_drive ;  F_peak = F_mean / Φ
// Tier A (senza L_drive): F_ref = EMA(F_peak ultime N vogate); confronto solo su forma/timing.
d        = clamp(0.33 + 0.0075*(R-20), 0.33, 0.46)   // frazione drive del ciclo
t_drive  = d * T ;  t_peak = u* * t_drive
RFD_tgt  = F_peak / t_peak    [N/s]
catch_factor_target ∈ [-35,-15] ms (Tier B) ;  RSF_target = 0.90 (Tier B)
```

### 3.4 Note sul campionamento a 100 Hz
- Buffer vogata: max ~4 s → **400 campioni** (forza). Usa buffer circolare fisso, es. `float[512]` per canale.
- **RFD**: NON differenza a due punti. Fai **regressione lineare** della forza sul tratto attacco→picco (riduce il rumore del 100 Hz).
- **Velocità (Tier B)**: differenzia `handle_pos`/`seat_pos` DOPO smoothing (media mobile o Savitzky–Golay corto), data la quantizzazione e i 10 ms di passo.
- **catch_factor**: zero-crossing della velocità con interpolazione lineare tra campioni (risoluzione nativa 10 ms insufficiente).

---

## 4. ESTRAZIONE FEATURE (device + host, da `coach_model`)

Segmentazione: riusa le **soglie Kg** esistenti (drive start / recovery) per delimitare drive e recovery. Per ogni vogata calcola le feature del Tier attivo (§2).
- Curva di forza normalizzata `F_norm(u)`: ricampiona il drive su griglia comune in u∈[0,1], normalizza a media 1.
- `n_secondary_peaks`: picchi prominenti oltre il principale (disconnessione gambe-busto-braccia).
- Costanza: SD di `L_drive`/durata drive tra vogate (marker di abilità).

---

## 5. FIRMWARE — INTEGRAZIONE NEL PROGETTO

Crea un modulo dedicato (`coach.h`/`coach.cpp`, o tab `coach.ino`) per non gonfiare `SmartRowerPro.ino`.

### 5.1 Calibrazione (Tier B)
Nuova azione nella tab **SETUP & PROFILE** della Web App: acquisisci 3–5 vogate lente, rileva attacco/finale via zero-crossing di `v_handle`, salva `x_catch,x_finish,L_drive` in `Preferences`. Valida contro `0.78*H`.

### 5.2 Motore curva target
Al cambio di (`H`/calibrazione, `P̄`/`R`, `preset`) ricalcola i parametri §3 e la LUT target. Tieni in RAM la LUT (64–128 float).

### 5.3 Hook a fine vogata
Nel punto in cui la macchina a stati attuale chiude la vogata (soglia recovery), chiama `coach_evaluate(stroke_buffer)`: calcola feature → scoring (§6) → cue. **Lavoro leggero, dopo il fine-colpo, senza bloccare il path BLE FTMS.** Latenza < 50 ms.

### 5.4 Web App (`webapp.h`)
- **Overlay curva target** sul plot di forza esistente (riusa l'estetica del "Ghost Pacer"): linea tratteggiata = `F_target(u)` scalata a `F_ref`/`F_peak`, sopra la curva reale.
- **Pannello cue**: mostra il cue prioritario + 3 numeri (es. Φ, posizione picco %, e RSF% in Tier B).
- Nuova `fetch` periodica a un endpoint JSON `/coach` (sotto) per aggiornare overlay e cue.
- Tab/sezione **Calibrazione** (Tier B) e selettore preset/(p,q).

### 5.5 Endpoint ESPAsyncWebServer (nuovi)
```
GET  /coach            -> JSON: feature ultima vogata + sotto-punteggi + cue + LUT target
GET  /coach/config     -> JSON: parametri correnti (preset,p,q,P̄,R,geometria)
POST /coach/config     -> aggiorna parametri (salva in Preferences)
POST /coach/calibrate  -> avvia/termina calibrazione (Tier B)
GET  /coach/csv        -> stream CSV della sessione (vedi 5.6)
```

### 5.6 Logging CSV per l'host
Dato il partition 4MB+SPIFFS, **non** salvare lunghe sessioni in flash. Preferisci **stream live al browser** (SSE/WebSocket o chunked su `/coach/csv`) con salvataggio lato client; in alternativa SD se presente (chiedi §0d). Schema:
```
t_ms,force_N[,handle_pos_m,seat_pos_m]      // colonne posizione solo in Tier B
# header metadati: height_m,weight_kg,target_power_W,target_rate_spm,preset,p,q,
#                  calib_x_catch_m,calib_x_finish_m,sample_rate_hz=100,tier=A|B
```

### 5.7 Vincoli da NON violare
BLE FTMS e compatibilità EXR/Kinomap intatte; OTA `/update` intatto; nessuna regressione su SPM/Watt/distanza esistenti; build con `sketch.yaml`/`arduino-cli` invariata (eventuali nuove librerie Tier B vanno aggiunte a `sketch.yaml`).

---

## 6. SCORING E CUE (device + host)

### 6.1 Sotto-punteggio per metrica
```
score_m = max(0, 100 * (1 - |meas - target| / tol_m))
```
Tolleranze (config): Φ ±0.04, RFD ±15%, peak_pos ±5pp, drive_recovery ±0.04, (Tier B) RSF ±8pp, L_drive ±5%, F_peak ±8%. catch_factor: 100 se in [-35,-15] ms, decresce fuori.

### 6.2 Punteggio di forma
RMSE tra `F_norm(u)` e `phi(u)` su griglia comune → `score_shape = 100*(1 - RMSE/RMSE_max)` (es. RMSE_max=0.6).

### 6.3 Logica del cue (priorità, NON somma)
Restituisci il cue del PRIMO gruppo con sotto-punteggio < soglia (es. 70):
1. **Sequenza/timing**: (Tier B) RSF, catch_factor; drive_recovery_ratio.
2. **Carico**: RFD, posizione del picco.
3. **Forma**: fullness, score_shape, n_secondary_peaks.

Dizionario difetto→cue (IT):

| condizione | cue |
|---|---|
| (B) RSF > 1.00 | "Shooting the slide: spingi di gambe finché il manubrio non segue il sedile" |
| (B) RSF < 0.80 | "Apri il busto troppo presto: lascia lavorare prima le gambe" |
| (B) catch_factor > -15 ms | "Attacco statico: inverti la direzione attaccando, non da fermo" |
| (B) catch_factor < -50 ms | "Il sedile scappa: carica l'attacco prima di estendere le gambe" |
| RFD basso | "Aggancia subito all'attacco, applica forza prima" |
| picco tardivo | "Anticipa lo sforzo: il massimo nella prima metà" |
| Φ basso (curva a spillo) | "Mantieni la connessione per tutta la passata" |
| n_secondary_peaks ≥ 1 | "Fondi la spinta delle gambe con l'apertura del busto" |
| drive_recovery troppo basso | "Rallenta il ritorno, prepara il busto a metà recupero" |

---

## 7. ANALIZZATORE CSV (HOST, PYTHON)

- CLI: `analyze.py session.csv` (legge l'header metadati; rileva Tier A/B dalle colonne).
- Importa lo stesso `coach_model` del firmware (coerenza verificata sui test).
- Output: (a) tabella per-vogata (feature + sotto-punteggi), (b) summary sessione (medie, trend, costanza), (c) top-3 cue ricorrenti, (d) grafici (curva media reale vs target in u; Tier B: v_handle/v_seat, RSF, catch_factor), (e) JSON re-importabile.
- **Hook Stadio 2 (personalizzazione):** smoothing B-spline (~25 basi) di `F_norm(u)`, registrazione sui landmark (attacco, picco, finale), fPCA per i modi di variazione dell'atleta, clustering vogate buone/cattive. Confronto col **proprio** ottimo, non con un ideale di popolazione.

---

## 8. TEST E ACCETTAZIONE

1. **Self-test sintetico (host):** genera `F_target` da (preset, R, e in Tier B H/P̄), aggiungi rumore + jitter a 100 Hz, verifica recupero di Φ, peak_pos, RFD (e Tier B: L_drive, RSF) entro tolleranza.
2. **Difetti iniettati:** strokes con shooting-the-slide (Tier B), apertura precoce busto, picco tardivo, doppio picco → cue atteso corretto.
3. **Coerenza device↔host:** stessi input → stesse feature entro tolleranza (test C++↔Python).
4. **Smoke test firmware:** compila con `arduino-cli`/`sketch.yaml`; verifica che BLE FTMS, Web App, SPM/Watt e OTA restino funzionanti (nessuna regressione); RAM residua sufficiente.
5. **Robustezza:** vogate parziali, saturazione cella, (Tier B) drop campioni e disallineamento temporale encoder/ToF (resampling a griglia 100 Hz).

---

## 9. STRUTTURA DELLA CONSEGNA

```
SmartRowerPro.ino     # hook a fine-vogata nella state machine esistente
coach.h / coach.cpp   # modulo coach on-device (Tier A sempre, Tier B sotto flag)
coach_model.h         # modello condiviso (forma Beta, scoring) — fonte di verità
webapp.h              # overlay target + pannello cue + tab calibrazione + fetch /coach
sketch.yaml           # eventuali librerie Tier B (encoder/ToF) aggiunte qui
/analyzer/            # analyze.py, coach_model.py (port), report, fPCA Stadio 2
/tests/               # self-test sintetici, difetti iniettati, coerenza C++/Python
/docs/                # formule §3, procedura calibrazione, schema CSV
```

Procedi a stadi: **(1)** `coach_model` + test host; **(2)** analizzatore Python (più facile da validare sui CSV); **(3)** firmware Tier A (solo forza, integrazione in `.ino`/`webapp.h`, nessuna regressione); **(4)** firmware Tier B quando avrò cablato encoder+ToF e risposto alla §0(b). Mantieni separato il livello deterministico/interpretabile da qualsiasi futura estensione ML. Fammi ora le domande della §0.
