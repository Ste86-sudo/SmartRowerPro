# Analizzatore sessione (Stadio 2)

Analisi post-sessione dei CSV esportati dalla web UI, con lo stesso modello
matematico del coach on-device (curva Beta, scoring §6, cue prioritario) più
la fPCA per la personalizzazione: i *tuoi* modi di variazione della curva,
non un ideale di popolazione.

## Uso

```
pip install numpy matplotlib
python analyze.py SmartRower_Export_2026-07-02.csv --pull 3.0 --rel 1.5 --height 181
```

Opzioni: `--preset auto|technique|race`, `--out cartella_report`.

## Output (in `<csv>_report/`)

- `report.txt` — riepilogo sessione, feature medie ± SD, punteggi, top-3 cue
- `session.json` — dati per-vogata re-importabili
- `features.csv` — feature + curva normalizzata (64 punti) per ogni vogata
- `curva_media.png` — curva media ± SD vs target Beta
- `trend_feature.png` — picco, Φ, RFD, durata drive per vogata
- `fpca_modi.png` — primi 2 modi fPCA (media ± 2σ)

## Self-test

`tools/analyzer` è stato validato con una sessione sintetica (40 vogate da
Beta(2,3) + rumore a 100 Hz): recupera picco al 36% (teorico 33%), Φ 0.60
(teorico 0.56 sulla curva pura), e il difetto RSF iniettato nella geometria
sedile/cavo produce il cue atteso.
