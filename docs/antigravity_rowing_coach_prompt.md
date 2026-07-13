# SUPER-PROMPT PER ANTIGRAVITY
## Coach virtuale per ergometro strumentato: motore di curve target (firmware) + analizzatore CSV post-sessione

---

## 0. RUOLO E OBIETTIVO

Agisci come ingegnere senior embedded + data engineer. Devi produrre **due deliverable** che condividono un **unico modulo matematico** (single source of truth, da NON duplicare):

1. **FIRMWARE** del vogatore: una routine di calibrazione per atleta, un motore che genera/memorizza le **curve di forza target** parametriche, e un feedback in tempo reale per-vogata.
2. **ANALIZZATORE CSV** (host, Python): legge i log di sessione, estrae le feature per ogni vogata, le confronta con le curve target, assegna punteggi e restituisce **un cue prioritario** + report di sessione.

Prima di scrivere codice, **fammi queste domande** se le risposte non sono nel contesto: (a) MCU/piattaforma e toolchain del firmware attuale (es. ESP32/STM32, C/C++/Rust, RTOS?); (b) come sono esposti oggi i tre sensori al firmware (driver/bus); (c) se il feedback live deve uscire su display, audio, o BLE verso app; (d) versione Python target per l'analizzatore. Poi procedi.

---

## 1. CONTESTO HARDWARE E SEGNALI

L'ergometro è strumentato con tre sensori:
- **Cella di carico wireless** sul manubrio → forza, campionata a **1000 Hz**.
- **Encoder** sulla corda/manubrio → spostamento del manubrio `handle_pos` [m].
- **Laser time-of-flight** → posizione del seggiolino `seat_pos` [m].

Convenzione segni: `handle_pos` e `seat_pos` crescono durante la passata (drive) e calano durante il recupero. All'attacco (catch) entrambi sono al minimo; al finale (finish) al massimo. Allinea i due encoder allo stesso istante temporale (interpola alla griglia 1 kHz se i rate nativi differiscono).

---

## 2. MODULO MATEMATICO CONDIVISO (`biomech_model`)

Implementa questo modulo UNA volta (es. C header-only condivisibile + porting Python, oppure libreria core che entrambi importano). Tutte le costanti sono parametri configurabili, non magic number.

### 2.1 Geometria per atleta
```
L_drive_nominale = 0.78 * H            // H = statura [m]; banda valida 0.75–0.82*H
L_seat  = 0.33 * L_drive               // contributo gambe (atteso)
L_upper = 0.67 * L_drive               // contributo tronco+braccia (atteso)
```
La geometria REALE viene però fissata dalla calibrazione (sez. 4): `x_catch`, `x_finish`,
`L_drive = x_finish - x_catch`. L'altezza serve solo a inizializzare e a validare
(allarme se L_drive_misurata < 0.70*H → "non raggiungi l'attacco").

### 2.2 Forma della curva (famiglia Beta su frazione di passata s ∈ [0,1])
```
s = (x - x_catch) / L_drive
phi(s; p, q) = s^(p-1) * (1-s)^(q-1) / Beta(p,q)     // normalizzata: ∫₀¹ phi ds = 1
peak_pos s* = (p-1)/(p+q-2)
fullness  Φ = 1 / phi(s*)
```
Preset (selezionabili da ritmo o da impostazione utente):
| preset            | p   | q   | picco | Φ    | range colpi |
|-------------------|-----|-----|-------|------|-------------|
| "technique"       | 2.0 | 3.0 | 33%   | 0.56 | 18–26       |
| "race"            | 2.0 | 2.5 | 40%   | 0.61 | 30–36       |
Permetti override manuale di (p, q).

### 2.3 Magnitudo e timing (da potenza target P̄ [W] e ritmo R [spm])
```
T        = 60 / R                              // periodo colpo [s]
W_stroke = 60 * P̄ / R                          // lavoro per vogata [J]
F_mean   = W_stroke / L_drive                  // forza media drive [N]
F_peak   = F_mean / Φ                           // [N]
d        = clamp(0.33 + 0.0075*(R-20), 0.33, 0.46)   // frazione drive del ciclo
t_drive  = d * T
t_peak   = s* * t_drive
RFD_tgt  = F_peak / t_peak                      // [N/s]
catch_factor_target ∈ [-35, -15] ms             // il sedile inverte PRIMA del manubrio
RSF_target = 0.90                               // sedile:manubrio nel primo 20% del drive
```

### 2.4 Curva di forza target completa
```
F_target(x) = F_mean * phi((x - x_catch)/L_drive; p, q),   x ∈ [x_catch, x_finish]
```
Il confronto misura-vs-target avviene nello spazio della POSIZIONE (frazione s), che è
indipendente dal ritmo. I timing (RFD, catch factor, rapporto drive:recovery) si calcolano
nel dominio del TEMPO.

---

## 3. SCHEMA DATI (CSV)

### 3.1 Log di sessione (`session_*.csv`), 1000 righe/s
```
t_ms,force_N,handle_pos_m,seat_pos_m
```
Header di metadati come commenti `#` in cima, oppure file `.json` affiancato:
```
# athlete_height_m=1.83
# target_power_W=220
# target_rate_spm=24
# drag_factor=120
# preset=technique
# calib_x_catch_m=..., calib_x_finish_m=...
```

### 3.2 File di calibrazione (`calib_*.csv`): stesso schema, 3–5 vogate lente.

---

## 4. FIRMWARE — REQUISITI

### 4.1 Routine di calibrazione
- Acquisisce N vogate lente, rileva attacco/finale tramite **zero-crossing della velocità del manubrio** (v_handle = d/dt handle_pos: neg→pos = catch, pos→neg = finish).
- Calcola e memorizza in NVM: `x_catch` (mediana dei minimi), `x_finish` (mediana dei massimi), `L_drive`, e l'altezza `H`.
- Valida contro il prior `0.78*H`; emetti warning se fuori banda.

### 4.2 Motore curve target
- Dato (`H`/calibrazione, `P̄`, `R`, `preset`) calcola tutti i parametri della sez. 2 e tiene in memoria la `F_target(x)` campionata (LUT su ~64–128 punti in s, interpolazione lineare).
- Ricalcola `F_mean`, `F_peak`, `RFD_tgt` ad ogni cambio di `R` o `P̄` (anche on-the-fly se l'andatura deriva).

### 4.3 Feedback real-time per-vogata
A fine di ogni vogata (rilevata via zero-crossing) calcola le feature della sez. 5 sul colpo
appena concluso e applica il motore di scoring (sez. 6). Output minimo: **il singolo cue
prioritario** + 2–3 numeri chiave (es. RSF%, posizione picco%, fullness). Latenza < 50 ms
dopo il finale. Vincoli: nessuna allocazione dinamica nel loop di controllo; usa buffer
circolari a dimensione fissa per il colpo corrente (max ~ 1 kHz * 4 s = 4000 campioni).

### 4.4 Logging
Scrivi il CSV di sessione (sez. 3.1) su storage/stream BLE per l'analizzatore.

---

## 5. ESTRAZIONE FEATURE (firmware e analizzatore, da `biomech_model`)

Segmentazione vogate: zero-crossing di `v_handle`. Per ogni vogata calcola:

**Solo forza:** `F_peak`, `F_mean` (solo drive), `Φ_meas = F_mean/F_peak`,
`RFD_meas` (pendenza attacco→picco), `t_to_peak`, `impulse = ∫F dt`,
`n_secondary_peaks` (conta picchi prominenti oltre il principale → disconnessione gambe-busto-braccia).

**Forza + posizione manubrio:** `work = ∫F dx`, `L_drive_meas`, `power`,
`v_handle(t)`, `peak_force_pos_pct = (x_peak - x_catch)/L_drive`,
e la **curva normalizzata** `F_norm(s)` (registra su s∈[0,1], normalizza a media 1).

**Manubrio + seggiolino:**
```
RSF_meas      = Δseat / Δhandle  nel primo 20% del drive (catch → transition)
upper_contrib(t) = (handle_pos - x_catch) - (seat_pos - seat_catch)   // tronco+braccia
catch_factor  = t(inversione seat) - t(inversione handle)  [ms]  (atteso negativo)
drive_recovery_ratio = t_drive / t_recovery
v_seat(t), gap v_seat - v_handle a inizio drive
```

**Cross-sessione/ritmo:** deviazione standard di `L_drive_meas` tra colpi e tra ritmi diversi
(costanza = marker di abilità; elite < 1 cm di SD).

---

## 6. MOTORE DI SCORING E CUE

### 6.1 Sotto-punteggio per metrica
```
score_m = max(0, 100 * (1 - |meas - target| / tol_m))
```
Tolleranze (configurabili): F_peak ±8%, F_mean ±6%, Φ ±0.04, RFD ±15%,
peak_force_pos ±5pp, RSF ±8pp, L_drive ±5%. Per il catch_factor: 100 se dentro
[-35,-15] ms, decrescente fuori banda.

### 6.2 Punteggio di forma
Registra `F_norm(s)` su griglia comune, calcola `RMSE` vs `phi(s)`:
```
score_shape = 100 * (1 - RMSE / RMSE_max)   // RMSE_max configurabile, es. 0.6
```

### 6.3 Logica del cue (priorità, NON somma)
Valuta i gruppi in quest'ordine e restituisci il cue del PRIMO gruppo che ha un
sotto-punteggio sotto soglia (es. < 70):
1. **Sequenza/timing**: RSF, catch_factor, drive_recovery_ratio.
2. **Carico**: RFD, posizione del picco.
3. **Forma**: fullness, score_shape, picchi secondari.

Dizionario difetto→cue (esteso/i18n, italiano):
| condizione | cue |
|---|---|
| RSF > 1.00 | "Shooting the slide: spingi di gambe finché il manubrio non segue il sedile" |
| RSF < 0.80 | "Apri il busto troppo presto: lascia lavorare prima le gambe" |
| catch_factor > -15 ms | "Attacco statico: inverti la direzione attaccando, non da fermo" |
| catch_factor < -50 ms | "Il sedile scappa: carica l'attacco prima di estendere le gambe" |
| RFD basso | "Aggancia la pedana subito all'attacco" |
| picco tardivo | "Anticipa lo sforzo: il massimo va nella prima metà" |
| Φ basso (curva a spillo) | "Mantieni la connessione per tutta la passata" |
| n_secondary_peaks ≥ 1 | "Fondi la spinta delle gambe con l'apertura del busto" |
| drive_recovery troppo basso | "Rallenta il ritorno, prepara il busto a metà recupero" |

---

## 7. ANALIZZATORE CSV (HOST, PYTHON) — REQUISITI

- CLI: `analyze.py session.csv [--meta meta.json] [--report out.html]`.
- Importa lo STESSO `biomech_model` del firmware (porting Python verificato bit-coerente sui casi di test).
- Produce: (a) **tabella per-vogata** (tutte le feature + sotto-punteggi), (b) **summary di sessione** (medie, trend, costanza tra colpi), (c) **top-3 cue ricorrenti** della sessione, (d) **grafici**: curva di forza media misurata vs target (in s), profili `v_handle`/`v_seat`, RSF e catch_factor nel tempo.
- **Hook Stadio 2 (personalizzazione, opzionale):** smoothing B-spline (~25 basi) di ogni `F_norm(s)`, registrazione sui landmark (catch, picco, finish), fPCA per estrarre i modi di variazione dell'atleta, clustering vogate "buone vs cattive" (per potenza o RSF). Confronta l'atleta col **proprio** ottimo, non con un ideale di popolazione.
- Output anche in JSON per re-import.

---

## 8. TEST E CRITERI DI ACCETTAZIONE

1. **Self-test sintetico**: genera una `F_target` dal modello con (H, P̄, R, preset) noti, aggiungi rumore gaussiano e jitter di timing, verifica che l'analizzatore recuperi `F_peak`, `Φ`, `peak_pos`, `RFD`, `L_drive` entro le tolleranze.
2. **Difetti iniettati**: sintetizza vogate con "shooting the slide" (sedile anticipato), apertura precoce del busto, picco tardivo, doppio picco → verifica che il motore emetta il cue corretto.
3. **Coerenza firmware↔host**: stessi input → stesse feature entro tolleranza numerica (test cross-language).
4. **Determinismo e riproducibilità**; copertura unit-test del modulo matematico ≥ 90%.
5. **Robustezza segnale**: gestisci colpi parziali, drop di campioni, disallineamento temporale dei due encoder (resampling), saturazione cella di carico.

---

## 9. STRUTTURA DELLA CONSEGNA

```
/biomech_model/        # core condiviso (C header-only + porting Python), unica fonte di verità
/firmware/             # calibrazione, motore curve target, feedback live, logging
/analyzer/             # analyze.py, report, hook fPCA stadio 2
/tests/                # self-test sintetici, difetti iniettati, coerenza cross-language
/docs/                 # README con le formule della sez.2 e istruzioni di calibrazione
```

Procedi a stadi: prima `biomech_model` + test, poi analizzatore (più facile da validare),
poi firmware. Mantieni il livello deterministico/interpretabile separato da qualsiasi
estensione ML futura, così i consigli restano spiegabili. Fammi le domande della sez. 0 ora.
