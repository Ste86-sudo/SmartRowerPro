# SPECIFICA FISICA — Generatore di ghost curves, firmware SmartRowerPro (V-Fit Tornado, aria)

Documento per l'agente di sviluppo firmware. Descrive **la fisica** da implementare per generare le curve target (ghost). Vincoli: non degradare BLE FTMS / Web App / OTA esistenti; matematica leggera on-device (solo `<math.h>`, niente alloc nel loop); l'analisi pesante (B-spline/fPCA) resta sull'host.

---

## 0. PRINCIPI (leggere prima di tutto)

1. **La forma della ghost nasce dalla CINEMATICA, non dalla macchina.** La forza è una *conseguenza* di come si muove il manubrio. Si prescrive il movimento (ritmo + sequenza dei segmenti), la forza esce dal modello macchina.
2. **La forza vede solo la SOMMA delle velocità dei segmenti.** `F = b₂·v_manubrio²` e `v_manubrio = v_gambe + v_tronco + v_braccia`. Quindi la ghost di forza è **cieca alla coordinazione**: stessa forza, coordinazioni diverse. Per questo si generano **tre ghost accoppiate** (forza, sellino, braccia): la forza dice *quanto/che forma*, sellino e braccia dicono *come/quando*.
3. **Si imposta il RITMO (SPM + drive:recupero), non la potenza.** La durata del drive è funzione di SPM e ritmo, **non** della taratura `b₂`. `b₂` scala l'ampiezza della forza e serve a *leggere* i watt; la potenza è un **output**, non un input. (Esiste anche la modalità a potenza-target, §6, ma non è la default.)
4. **Macchina = aria quasi pura.** Misurato sul Tornado: `F ≈ b₂·v²`, con inerzia (`M_eff`) e corda trascurabili. Una sola costante macchina significativa: `b₂`.

---

## 1. MODELLO FISICO DELLA MACCHINA

Forza al manubrio (al netto della cinematica):
```
F(t) = b₂·v(t)²  +  M_eff·a(t)  +  F_cord(x)
```
- `b₂·v²`  — resistenza aria (∝ velocità²). **Termine dominante.** `b₂ ≈ 110 N·s²/m²` (provvisorio, intervallo 100–125).
- `M_eff·a` — **reattività** del volano (inerzia che "rincorre" la mano): aggiunge forza durante l'accelerazione, modifica la **forma** (più front-loaded) ma **non** la durata. Misurato `M_eff ≈ 0` (ventola PU leggera → reagisce all'istante). **Tieni il termine nel modello con `M_eff` parametro, default 0.**
- `F_cord(x)` — corda di ritorno: misurata trascurabile (`k≈0`, `F0≈18 N`). Opzionale, default off.

Quindi per il Tornado, in pratica: **`F = b₂·v²`**.

Unità: la cella riporta **kg**; convertire in **N** (`F_N = F_kg · 9.81`) all'ingresso. `v` in m/s, `x` in m, `a` in m/s².

---

## 2. COORDINATE, SEGNALI, EVENTI

Tre segnali a 100 Hz: forza `F`, posizione manubrio `x_h` (encoder), posizione sellino `x_s` (ToF).
- Convenzione: `x_h`, `x_s` crescono nel **drive**, calano nel **recupero**. All'attacco minimi, al finale massimi.
- Derivate (dopo smoothing): `v_h = dx_h/dt`, `a_h = d²x_h/dt²`, `v_s = dx_s/dt`.
- **Eventi:** attacco = inversione (minimo) di `x_h` / salita forza sopra soglia drive; finale = massimo di `x_h`.
- **Drive** = attacco→finale; **recupero** = finale→attacco successivo.

Decomposizione (chiave): lo spostamento del manubrio è la somma dei contributi dei segmenti; il sellino misura le sole gambe:
```
x_h = s_gambe + s_tronco + s_braccia
x_s = s_gambe                         → contributo corpo-alto = x_h − x_s = s_tronco + s_braccia
v_h = v_gambe + v_tronco + v_braccia  (v_gambe = v_s)
```

---

## 3. CINEMATICA IDEALE (il template — sorgente della FORMA)

Profilo minimum-jerk per ogni segmento, sequenziato gambe→tronco→braccia.
```
S(p)   = 10p³ − 15p⁴ + 6p⁵        // spostamento normalizzato 0→1
S'(p)  = 30p²(1−p)²               // velocità (campana, ∫=1)
S''(p) = 60p(1−p)(1−2p)           // accelerazione (=0 ai bordi)
```
Spostamento di ogni segmento sul drive (τ ∈ [0,1]):
```
A_gambe  = f_gambe ·L_drive   (≈0.33)   τ_on,off = 0.00 , 0.60
A_tronco = f_tronco·L_drive   (≈0.31)   τ_on,off = 0.30 , 0.85   // entra al "transition point"
A_braccia= f_braccia·L_drive  (≈0.36)   τ_on,off = 0.55 , 1.00
p_i(τ) = clamp((τ − τ_on,i)/(τ_off,i − τ_on,i), 0, 1)
s_i(τ) = A_i · S(p_i(τ))
```
Le frazioni `f_i` e i tempi `τ_on/τ_off` sono **parametri** (sequenza), calibrabili confrontando i ghost di sellino e corpo-alto con i segnali reali `x_s` e `x_h−x_s`.

Aggregati e profilo normalizzato della velocità del manubrio:
```
x_h(τ) = Σ s_i(τ)
v_hat(τ) = Σ A_i·S'(p_i)/(τ_off,i−τ_on,i)        // velocità manubrio non scalata
ψ(τ)   = v_hat(τ) / ∫₀¹ v_hat dτ                  // normalizzata: ∫₀¹ ψ dτ = 1
ψ_max  = max ψ(τ)
I3     = ∫₀¹ ψ(τ)³ dτ
```
Calcola `ψ(τ)`, `ψ_max`, `I3` **una volta sola** (numericamente) e ai cambi di sequenza.
Vincolo di forma (validazione): la ghost di forza risultante deve avere **picco al 33–40% del drive** e **fullness (medio/picco) 0.55–0.65**. Se fuori, aggiusta `τ`/`f_i`, **non** imporre una forma analitica.

---

## 4. PARAMETRIZZAZIONE — input/output (RITMO-first)

**INPUT:** `R` [spm], ritmo `d` (frazione di drive del ciclo), `L_drive` [m], template (`ψ, ψ_max, I3`), `b₂` [, `M_eff`].
**OUTPUT:** le tre ghost + potenza risultante.

```
T       = 60 / R                          // periodo del colpo [s]
t_drive = d · T                           // durata drive — FUNZIONE DI SPM E RITMO, non di b₂
t_rec   = T − t_drive
```
Ritmo di default `d(R)` (calibrabile sul rower): `d ≈ clamp(0.33 + 0.0075·(R−20), 0.33, 0.50)` (1:2 a basso rate, comprime verso 1:1 in alto).

```
v_h(τ)  = (L_drive / t_drive) · ψ(τ)                       // velocità ideale manubrio [m/s]
a_h(τ)  = (L_drive / t_drive²) · ψ'(τ)                     // ψ' = dψ/dτ (per il termine reattività)
```

**Potenza risultante (OUTPUT, per leggere i watt):**
```
P = (R/60) · b₂ · L_drive³ · I3 / t_drive²      [W]
```
→ `b₂` entra **solo** qui (lettura potenza) e nell'ampiezza forza (§5). Non tocca `t_drive`.

---

## 5. GENERAZIONE DELLE TRE GHOST (in tempo)

```
// 1) FORZA (aggregato) — dipende solo dalla SOMMA delle velocità
F_ghost(τ) = b₂ · v_h(τ)²  +  M_eff · a_h(τ)          // M_eff=0 sul Tornado → b₂·v_h²
F_ghost(τ) = max(F_ghost(τ), 0)                      // clamp (chain slack al finale)
F_peak     = max F_ghost(τ)

// 2) SELLINO (gambe) — coordinazione
x_s_ghost(τ) = s_gambe(τ)
v_s_ghost(τ) = (1/t_drive) · A_gambe·S'(p_gambe)/(Δτ_gambe)

// 3) BRACCIA (e tronco) — timing chiusura
x_arm_ghost(τ) = s_braccia(τ)         (+ s_tronco per il corpo-alto)
v_arm_ghost(τ) = (1/t_drive) · A_braccia·S'(p_braccia)/(Δτ_braccia)
```
Le tre condividono lo stesso `t_drive`: cambiando SPM/ritmo si **riscalano insieme** sul tempo. La forza è la somma; sellino/braccia sono le parti (per coachare la sequenza: shooting-the-slide = `v_s` alta ma `v_h` bassa; apertura busto precoce; braccia anticipate).

Asse di confronto per l'overlay: **posizione** `u = (x − x_catch)/L_drive` per la curva forza-posizione; **tempo** per i confronti di velocità/sequenza.

---

## 6. DIPENDENZA DAGLI SPM (leggi di scala)

A **`L_drive` costante** (è la tecnica corretta: salendo di colpi si accorcia il *recupero*, non la passata).

### Default RITMO-first (imposti R e d; potenza = output)
| grandezza | scala |
|---|---|
| `t_drive = d·60/R` | ∝ 1/R (a d cost.) |
| `v_peak ∝ L/t_drive` | **∝ R/d** |
| **`F_peak = b₂·v_peak²`** | **∝ (R/d)²** |
| **Potenza (output)** | **∝ R³/d²** (legge del cubo) |
| forma normalizzata | invariata |

→ Spingere lo stesso colpo più veloce (R↑ a ritmo dato) → forza ~quadratica, potenza ~cubica nel ritmo.

### Modalità alternativa POTENZA-target (imposti R e P̄; risolvi t_drive)
```
t_drive = sqrt( R · b₂ · L_drive³ · I3 / (60 · P̄) )    // QUI b₂ entra in t_drive
```
| a P̄ costante, R↑ | scala |
|---|---|
| `F_peak` | ∝ 1/R (curva più bassa) |
| `v_peak` | ∝ 1/√R |
| `t_drive` | ∝ √R ; `d` ∝ R^1.5 |

Default = ritmo-first. Usa potenza-target solo per programmi a watt prescritti.

---

## 7. IMPLEMENTAZIONE FIRMWARE

- Calcola `ψ, ψ_max, I3` una volta (e ai cambi di sequenza). Mantieni le **LUT** delle tre ghost (64–128 punti), ricalcolate ai cambi di `R, d, L_drive, b₂, M_eff`. Operazioni economiche (uno `sqrt`, moltiplicazioni): adatte all'ESP32.
- Persisti in `Preferences`: `b₂, M_eff, L_drive, x_catch, x_finish, d(R), f_i, τ_on/off`.
- Overlay `F_ghost(u)` sul plot forza-posizione (stile Ghost Pacer); aggiungi overlay `v_s_ghost`, `v_arm_ghost` vs i reali. Endpoint `GET /ghost` (JSON: 3 LUT + `R, d, L_drive, b₂, t_drive, P, F_peak`).
- Non toccare BLE FTMS / Web App / OTA.

### Pseudocodice
```
once: psi, psi_max, I3 = build_template(f_i, tau_on, tau_off)
on_change(R, d, L, b2, Meff):
    T=60/R; t_d=d*T
    P = (R/60)*b2*L^3*I3/t_d^2                       // watt risultanti (display)
    for i in 0..Nlut:
        tau = i/(Nlut-1)
        v   = (L/t_d)*psi(tau)
        a   = (L/t_d^2)*dpsi(tau)
        Fghost[i]  = max(b2*v*v + Meff*a, 0)
        vsGhost[i] = seat_velocity_template(tau, L, t_d)
        vaGhost[i] = arm_velocity_template(tau, L, t_d)
    Fpeak = max(Fghost)
loop@100Hz:
    use (force, x_h, x_s) for segmentation, overlay, live compare
```

---

## 8. VERIFICA / ACCETTAZIONE

1. **Cancellazione `b₂`:** in ritmo-first, variando `b₂` (50/110/200) a `R,d,L` fissi, `F_peak` invariato e `t_drive` invariato (cambia solo `P` letto). In potenza-target, `t_drive ∝ √b₂`.
2. **Scala `L_drive`:** `F_peak ∝ 1/L_drive`; estensione orizzontale = `L_drive`.
3. **Reattività:** con `M_eff>0` la forma si front-loada a parità di `t_drive` (durata invariata).
4. **Forma:** ghost di forza con picco 33–40%, fullness 0.55–0.65.
5. Determinismo, nessuna regressione su BLE/FTMS/Watt/OTA, RAM residua sufficiente.

---

## 9. SINTESI COSTANTI
| simbolo | significato | valore Tornado |
|---|---|---|
| `b₂` | resistenza aria | ≈110 N·s²/m² (100–125, provvisorio) |
| `M_eff` | inerzia/reattività | ≈0 |
| `F_cord, k` | corda di ritorno | ≈0 / F0≈18N (trascurabile) |
| `L_drive` | lunghezza passata | ~1.25 m |
| `ψ, ψ_max, I3` | forma velocità manubrio | dal template |
| `d(R)` | frazione di drive | ~0.33→0.50 |

**Regola d'oro:** forma ← cinematica; ampiezza forza ← `b₂`; durata ← SPM+ritmo; watt ← output via `b₂`; coordinazione ← sellino+braccia (non la forza).
