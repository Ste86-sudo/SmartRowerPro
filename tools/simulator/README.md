# Frame simulator

Serves the real web UI (`web/index.html`) and simulates the frame firmware
locally: realistic strokes with random technique faults, drifting HR, full
command handling (thresholds, encoder toggle, ghost, CFG). Python standard
library only — works offline.

```
python simulate.py               # http://localhost:8080
python simulate.py --fault      # + encoder-fault demo (120→150 s)
python simulate.py --port 9090
```

What to watch: live force/cord/seat curves and Peak Force on the Dashboard,
changing cues and catch factor on the Coach tab (the simulator injects late
peaks, spiky curves, double humps, shooting the slide, rushed recovery),
W′ balance draining above CP, HR zone bar filling, session save + ghost
replay in History, thresholds and athlete profile in Setup. With `--fault`,
a red ENCODER FAULT banner at 120 s and automatic fallback metrics.

Note: synthetic data — it exercises the UI, not the firmware physics.
