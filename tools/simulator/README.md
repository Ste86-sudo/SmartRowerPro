# Simulatore telaio

Serve la web UI vera (`web/index.html`) e simula il firmware del telaio in
locale: vogata realistica con difetti tecnici casuali, HR con deriva, comandi
gestiti (soglie, encoder toggle, ghost, CFG). Solo libreria standard Python,
funziona offline.

```
python simulate.py               # http://localhost:8080
python simulate.py --fault      # + demo guasto encoder (120→150 s)
python simulate.py --port 9090
```

Cosa osservare:

| Dove | Cosa |
|---|---|
| Dashboard | curve live forza/cavo/sedile, Forza Max Colpo, W′ Balance che scende sopra CP, barra zone HR che si riempie |
| Coach | cue che cambiano (il simulatore inietta picco tardivo, curva a spillo, doppio picco, shooting the slide, ritorno veloce), P(t) viola, catch factor, pannello Fatica |
| Storico | pannello sessione (kcal ×3, TRIMP, decoupling), SALVA SESSIONE → tabella + trend, poi SFIDA per il ghost replay |
| Setup | toggle "Metriche da Encoder" (il simulatore azzera il cavo), soglie via SALVA HW, parametri atleta |
| `--fault` | a 120 s banner rosso ENCODER GUASTO, rientro a 150 s |

Nota: il simulatore usa dati sintetici — serve a collaudare la UI, non la fisica
del firmware.
