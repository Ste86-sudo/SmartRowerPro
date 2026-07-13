#!/usr/bin/env python3
"""
SmartRower 2.0 — Analizzatore CSV post-sessione (Stadio 2 del super-prompt).

Rilegge l'export CSV della web UI, ricalcola le feature per vogata con lo
stesso modello del coach on-device (curva Beta, scoring, cue), produce un
report di sessione e l'analisi fPCA dei modi di variazione personali.

Uso:
    python analyze.py session.csv [--pull 3.0] [--rel 1.5]
                                  [--preset auto|technique|race]
                                  [--height 181] [--out report/]

CSV atteso (export "SmartRower_Export_*.csv" della UI):
    time,tw,ts,tf,rw,rf,cp,sp,hr
    time = ms dall'inizio registrazione, rf = forza (kg),
    cp = posizione cavo (m), sp = posizione sedile (m), rw = watt, hr = bpm

Dipendenze: numpy, matplotlib (pip install numpy matplotlib)
"""

import argparse
import csv
import json
import math
import os
import sys

import numpy as np

# Palette (colonna light validata della skill dataviz)
C_SERIES = "#2a78d6"   # blu — serie misurata
C_TARGET = "#eda100"   # giallo — curva target
C_NEG = "#e34948"      # rosso — modo PCA negativo
C_TEXT = "#52514e"
GRID = dict(color="#999999", alpha=0.18, linewidth=0.8)

N_GRID = 64  # griglia comune in u∈[0,1] (come il firmware/UI)


# ----------------------------- modello condiviso -----------------------------

def beta_lut(p, q, n=N_GRID):
    """phi(u;p,q) normalizzata a media 1 (normalizzazione numerica)."""
    u = np.linspace(1e-6, 1 - 1e-6, n)
    v = u ** (p - 1) * (1 - u) ** (q - 1)
    return v / v.mean()


def preset_pq(name, spm=20):
    if name == "auto":
        name = "race" if spm >= 28 else "technique"
    return (2.0, 2.5, "race") if name == "race" else (2.0, 3.0, "technique")


def drive_fraction_target(spm):
    return float(np.clip(0.33 + 0.0075 * (spm - 20), 0.33, 0.46))


def linreg_slope(x, y):
    x = np.asarray(x, float)
    y = np.asarray(y, float)
    if len(x) < 3:
        return 0.0
    den = (len(x) * (x * x).sum() - x.sum() ** 2)
    return float((len(x) * (x * y).sum() - x.sum() * y.sum()) / den) if den else 0.0


def score_of(meas, target, tol):
    return max(0.0, 100.0 * (1.0 - abs(meas - target) / tol))


# ----------------------------- lettura e segmentazione -----------------------------

def read_csv(path):
    rows = []
    with open(path, newline="", encoding="utf-8-sig") as f:
        rd = csv.reader(f)
        header = None
        for r in rd:
            if not r or r[0].startswith("#"):
                continue
            if header is None and not r[0].replace(".", "").replace("-", "").isdigit():
                header = [c.strip().lower() for c in r]
                continue
            rows.append(r)
    if header is None:
        header = ["time", "tw", "ts", "tf", "rw", "rf", "cp", "sp", "hr"]
    idx = {name: header.index(name) for name in header}

    def col(name, default=0.0):
        i = idx.get(name)
        if i is None:
            return np.full(len(rows), default)
        return np.array([float(r[i]) if i < len(r) and r[i] else default for r in rows])

    return {
        "t": col("time") / 1000.0,   # s
        "f": col("rf"),              # kg
        "c": col("cp"),              # m
        "s": col("sp"),              # m
        "w": col("rw"),              # watt
        "hr": col("hr"),
    }


def segment_strokes(d, pull, rel):
    """Segmenta i drive con le stesse soglie kg del firmware."""
    strokes = []
    pulling = False
    start = 0
    for i, fk in enumerate(d["f"]):
        if fk > pull and not pulling:
            pulling = True
            start = i
        elif fk < rel and pulling:
            pulling = False
            if i - start >= 8:
                strokes.append((start, i))
    return strokes


# ----------------------------- feature per vogata -----------------------------

def stroke_features(d, i0, i1, height_m, prev_end):
    t = d["t"][i0:i1]
    f = d["f"][i0:i1]
    c = d["c"][i0:i1]
    s = d["s"][i0:i1]
    n = len(f)
    drive_s = t[-1] - t[0]
    if drive_s <= 0.2:
        return None

    i_peak = int(np.argmax(f))
    f_peak = float(f[i_peak])
    f_mean = float(f.mean())
    if f_peak <= 0:
        return None

    feat = {
        "t0": float(t[0]),
        "drive_s": float(drive_s),
        "f_peak": f_peak,
        "f_mean": f_mean,
        "fullness": f_mean / f_peak,
        "peak_pos": i_peak / max(1, n - 1),
        "rfd": linreg_slope(t[: i_peak + 1] - t[0], f[: i_peak + 1] * 9.81) if i_peak >= 2 else 0.0,
        "recovery_s": float(t[0] - d["t"][prev_end]) if prev_end is not None else np.nan,
        "hr": float(np.mean(d["hr"][i0:i1])),
        "watts": float(np.mean(d["w"][i0:i1])),
    }

    # curva normalizzata (media = 1) su griglia comune
    u_src = np.linspace(0, 1, n)
    u_dst = np.linspace(0, 1, N_GRID)
    curve = np.interp(u_dst, u_src, f)
    m = curve.mean()
    feat["curve"] = curve / m if m > 0.01 else curve

    # Tier B se il cavo si muove davvero
    l_drive = float(c.max() - c.min())
    if l_drive > 0.3:
        dc = np.diff(c)
        feat["l_drive"] = l_drive
        feat["work_j"] = float((f[1:][dc > 0] * 9.81 * dc[dc > 0]).sum())
        k = max(2, int(n * 0.2))
        dh = c[k] - c[0]
        feat["rsf"] = float(abs(s[k] - s[0]) / dh) if dh > 0.01 else np.nan
        feat["l_target"] = 0.78 * height_m
    return feat


def stroke_scores_and_cue(feat, lut, p, q, spm):
    phi_target = 1.0 / lut.max()
    u_star = (p - 1) / (p + q - 2)
    d_tgt = drive_fraction_target(spm)
    cyc = feat["drive_s"] + (feat["recovery_s"] if not math.isnan(feat["recovery_s"]) else 0)
    d_meas = feat["drive_s"] / cyc if cyc > 0 and not math.isnan(feat["recovery_s"]) else d_tgt

    rmse = float(np.sqrt(((feat["curve"] - lut) ** 2).mean()))
    t_peak_tgt = u_star * d_tgt * (60.0 / max(spm, 10))
    rfd_tgt = feat["f_peak"] * 9.81 / max(t_peak_tgt, 0.1)

    sc = {
        "forma": max(0.0, 100 * (1 - rmse / 0.6)),
        "fullness": score_of(feat["fullness"], phi_target, 0.08),
        "peak_pos": score_of(feat["peak_pos"], u_star, 0.10),
        "ritmo": score_of(d_meas, d_tgt, 0.08),
        "rfd": score_of(feat["rfd"], rfd_tgt, rfd_tgt * 0.4) if rfd_tgt > 0 else 0.0,
    }
    if not math.isnan(feat.get("rsf", float("nan"))):
        sc["rsf"] = score_of(feat["rsf"], 0.90, 0.15)
    if "l_drive" in feat:
        sc["l_drive"] = score_of(feat["l_drive"], feat["l_target"], feat["l_target"] * 0.12)

    TH = 70
    cue = None
    if sc.get("rsf", 100) < TH:
        cue = ("Shooting the slide: spingi di gambe finché il manubrio non segue il sedile"
               if feat["rsf"] > 1.0 else
               "Apri il busto troppo presto: lascia lavorare prima le gambe")
    elif sc["ritmo"] < TH and d_meas > d_tgt:
        cue = "Rallenta il ritorno, prepara il busto a metà recupero"
    elif sc["rfd"] < TH and feat["rfd"] < rfd_tgt:
        cue = "Aggancia subito all'attacco, applica forza prima"
    elif sc["peak_pos"] < TH and feat["peak_pos"] > u_star:
        cue = "Anticipa lo sforzo: il massimo nella prima metà"
    elif sc["fullness"] < TH and feat["fullness"] < phi_target:
        cue = "Mantieni la connessione per tutta la passata"
    elif sc["forma"] < TH:
        cue = "Curva irregolare: cerca una spinta unica e progressiva"
    feat["rmse"] = rmse
    feat["d_meas"] = d_meas
    return sc, cue


# ----------------------------- fPCA (Stadio 2) -----------------------------

def fpca(curves):
    """PCA funzionale sulle curve normalizzate: media, modi, varianza spiegata."""
    X = np.asarray(curves)
    mean = X.mean(axis=0)
    Xc = X - mean
    U, S, Vt = np.linalg.svd(Xc, full_matrices=False)
    var = S ** 2 / max(1, len(X) - 1)
    explained = var / var.sum() if var.sum() > 0 else var
    scores = U * S
    return mean, Vt, np.sqrt(var), explained, scores


# ----------------------------- grafici -----------------------------

def make_plots(outdir, feats, lut, mean, modes, sdev, explained, scores):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    u = np.linspace(0, 1, N_GRID)
    curves = np.array([f["curve"] for f in feats])

    # 1. Curva media ± SD vs target Beta
    fig, ax = plt.subplots(figsize=(7, 4.2))
    sd = curves.std(axis=0)
    ax.fill_between(u, mean - sd, mean + sd, color=C_SERIES, alpha=0.15, linewidth=0)
    ax.plot(u, mean, color=C_SERIES, lw=2, label="Curva media misurata")
    ax.plot(u, lut, color=C_TARGET, lw=2, ls="--", label="Target Beta")
    ax.set_xlabel("u (frazione del drive)")
    ax.set_ylabel("Forza normalizzata (media = 1)")
    ax.set_title("Curva di forza media vs target")
    ax.grid(**GRID)
    ax.legend(frameon=False)
    for s_ in ("top", "right"):
        ax.spines[s_].set_visible(False)
    fig.tight_layout()
    fig.savefig(os.path.join(outdir, "curva_media.png"), dpi=140)
    plt.close(fig)

    # 2. Trend delle feature — small multiples (una serie per pannello, un solo asse)
    keys = [("f_peak", "Picco forza (kg)"), ("fullness", "Pienezza Φ"),
            ("rfd", "RFD (N/s)"), ("drive_s", "Durata drive (s)")]
    fig, axes = plt.subplots(2, 2, figsize=(9, 5.6), sharex=True)
    xs = np.arange(1, len(feats) + 1)
    for ax, (k, label) in zip(axes.flat, keys):
        ys = np.array([f[k] for f in feats])
        ax.plot(xs, ys, color=C_SERIES, lw=1.6)
        sl = linreg_slope(xs, ys)
        ax.plot(xs, ys.mean() + sl * (xs - xs.mean()), color=C_TEXT, lw=1, ls=":")
        ax.set_title(label, fontsize=10, color=C_TEXT)
        ax.grid(**GRID)
        for s_ in ("top", "right"):
            ax.spines[s_].set_visible(False)
    fig.suptitle("Trend per vogata (linea punteggiata = tendenza)", fontsize=11)
    fig.supxlabel("N. vogata")
    fig.tight_layout()
    fig.savefig(os.path.join(outdir, "trend_feature.png"), dpi=140)
    plt.close(fig)

    # 3. Modi fPCA: media ± 2σ·modo (blu = +, rosso = −)
    n_modes = min(2, len(modes))
    fig, axes = plt.subplots(1, n_modes, figsize=(4.6 * n_modes, 4.0), squeeze=False)
    for m in range(n_modes):
        ax = axes[0][m]
        pert = 2 * sdev[m] * modes[m]
        ax.plot(u, mean, color=C_TEXT, lw=2, label="Media")
        ax.plot(u, mean + pert, color=C_SERIES, lw=1.6, label="+2σ")
        ax.plot(u, mean - pert, color=C_NEG, lw=1.6, label="−2σ")
        ax.set_title(f"Modo {m + 1} ({explained[m] * 100:.0f}% varianza)", fontsize=10)
        ax.set_xlabel("u")
        ax.grid(**GRID)
        ax.legend(frameon=False, fontsize=8)
        for s_ in ("top", "right"):
            ax.spines[s_].set_visible(False)
    fig.suptitle("fPCA: i tuoi modi personali di variazione della curva", fontsize=11)
    fig.tight_layout()
    fig.savefig(os.path.join(outdir, "fpca_modi.png"), dpi=140)
    plt.close(fig)


# ----------------------------- main -----------------------------

def main():
    ap = argparse.ArgumentParser(description="Analizzatore sessione SmartRower")
    ap.add_argument("csv")
    ap.add_argument("--pull", type=float, default=3.0, help="soglia inizio tirata (kg)")
    ap.add_argument("--rel", type=float, default=1.5, help="soglia rilascio (kg)")
    ap.add_argument("--preset", default="auto", choices=["auto", "technique", "race"])
    ap.add_argument("--height", type=float, default=181.0, help="altezza atleta (cm)")
    ap.add_argument("--out", default=None, help="cartella report (default: <csv>_report)")
    args = ap.parse_args()

    outdir = args.out or os.path.splitext(args.csv)[0] + "_report"
    os.makedirs(outdir, exist_ok=True)

    d = read_csv(args.csv)
    if len(d["t"]) < 100:
        sys.exit("CSV troppo corto o non riconosciuto")

    segs = segment_strokes(d, args.pull, args.rel)
    if len(segs) < 3:
        sys.exit(f"Trovate solo {len(segs)} vogate: controlla le soglie --pull/--rel")

    feats = []
    prev_end = None
    for i0, i1 in segs:
        f = stroke_features(d, i0, i1, args.height / 100.0, prev_end)
        if f:
            feats.append(f)
            prev_end = i1
    print(f"Vogate valide: {len(feats)}")

    # SPM medio dalla durata ciclo
    cycles = [f["drive_s"] + f["recovery_s"] for f in feats if not math.isnan(f["recovery_s"])]
    spm = 60.0 / np.mean(cycles) if cycles else 20.0
    p, q, preset_name = preset_pq(args.preset, spm)
    lut = beta_lut(p, q)

    all_scores, cue_count = [], {}
    for f in feats:
        sc, cue = stroke_scores_and_cue(f, lut, p, q, spm)
        f["scores"] = sc
        f["cue"] = cue
        all_scores.append(sc)
        if cue:
            cue_count[cue] = cue_count.get(cue, 0) + 1

    # fPCA
    mean, modes, sdev, explained, pca_scores = fpca([f["curve"] for f in feats])

    make_plots(outdir, feats, lut, mean, modes, sdev, explained, pca_scores)

    # report testo + JSON
    def agg(k):
        v = np.array([f[k] for f in feats if k in f and not math.isnan(f.get(k, float("nan")))])
        return (v.mean(), v.std()) if len(v) else (float("nan"), float("nan"))

    top3 = sorted(cue_count.items(), key=lambda x: -x[1])[:3]
    keys_avg = ["f_peak", "fullness", "peak_pos", "rfd", "drive_s", "l_drive", "rsf", "rmse", "watts", "hr"]
    lines = [
        f"SmartRower — Report sessione: {os.path.basename(args.csv)}",
        f"Vogate: {len(feats)} — SPM medio: {spm:.1f} — preset target: {preset_name} (p={p}, q={q})",
        "",
        "Feature (media ± SD):",
    ]
    for k in keys_avg:
        mu, sd_ = agg(k)
        if not math.isnan(mu):
            lines.append(f"  {k:10s}: {mu:8.2f} ± {sd_:.2f}")
    lines.append("")
    lines.append("Punteggi medi:")
    for k in all_scores[0]:
        vals = [s[k] for s in all_scores if k in s]
        lines.append(f"  {k:10s}: {np.mean(vals):5.0f}")
    lines.append("")
    lines.append("Top-3 cue ricorrenti:")
    if top3:
        for cue, cnt in top3:
            lines.append(f"  [{cnt:3d}x] {cue}")
    else:
        lines.append("  Nessun difetto ricorrente — ottima sessione!")
    lines.append("")
    lines.append(f"fPCA: modo 1 spiega {explained[0] * 100:.0f}% della varianza"
                 + (f", modo 2 {explained[1] * 100:.0f}%" if len(explained) > 1 else ""))
    lines.append(f"Grafici e dati in: {outdir}")

    report = "\n".join(lines)
    print("\n" + report)
    with open(os.path.join(outdir, "report.txt"), "w", encoding="utf-8") as fp:
        fp.write(report + "\n")

    # JSON re-importabile (senza curve, che stanno in features.csv)
    with open(os.path.join(outdir, "session.json"), "w", encoding="utf-8") as fp:
        json.dump({
            "csv": os.path.basename(args.csv),
            "n_strokes": len(feats),
            "spm": round(spm, 1),
            "preset": preset_name,
            "top_cues": top3,
            "explained_variance": [round(float(x), 4) for x in explained[:4]],
            "strokes": [{k: (round(float(v), 4) if isinstance(v, (int, float)) and not (isinstance(v, float) and math.isnan(v)) else None)
                         for k, v in f.items() if k not in ("curve", "scores", "cue")} |
                        {"scores": {k: round(float(v), 1) for k, v in f["scores"].items()}, "cue": f["cue"]}
                        for f in feats],
        }, fp, ensure_ascii=False, indent=1)

    with open(os.path.join(outdir, "features.csv"), "w", newline="", encoding="utf-8") as fp:
        wr = csv.writer(fp)
        wr.writerow(["stroke", "t0_s"] + keys_avg + ["cue"] + [f"u{i}" for i in range(N_GRID)])
        for i, f in enumerate(feats):
            wr.writerow([i + 1, round(f["t0"], 2)]
                        + [round(f.get(k, float("nan")), 4) for k in keys_avg]
                        + [f["cue"] or ""]
                        + [round(float(x), 4) for x in f["curve"]])


if __name__ == "__main__":
    main()
