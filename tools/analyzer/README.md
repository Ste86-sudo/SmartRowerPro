# Session analyzer (Stage 2)

Post-session analysis of the CSV files exported by the web UI, using the same
mathematical model as the on-device coach (Beta target curve, tolerance
scoring, priority cue) plus **fPCA** for personalisation: *your* modes of
curve variation, not a population ideal.

## Usage

```
pip install numpy matplotlib
python analyze.py SmartRower_Export_2026-07-02.csv --pull 3.0 --rel 1.5 --height 181
```

Options: `--preset auto|technique|race`, `--out report_dir`.

## Output (in `<csv>_report/`)

- `report.txt` — session summary, mean features ± SD, scores, top-3 cues
- `session.json` — per-stroke data, re-importable
- `features.csv` — features + normalised curve (64 points) per stroke
- `curva_media.png` — mean curve ± SD vs Beta target
- `trend_feature.png` — peak, fullness, RFD, drive time per stroke
- `fpca_modi.png` — first 2 fPCA modes (mean ± 2σ)

## Self-test

Validated on a synthetic session (40 strokes from Beta(2,3) + 100 Hz noise):
recovers peak at 36% (theory 33%), fullness 0.60, and an injected RSF defect
produces the expected cue.
