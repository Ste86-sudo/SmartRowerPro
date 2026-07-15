"""generate_workouts.py — SmartRower Pro workout library generator.

Builds ~170 structured workouts in EXR (.xsr) and Zwift (.zwo) formats.
Every session is DESIGNED with the project's own models:
  - W' intervals: segment-by-segment simulation of Skiba W'-balance
    (Skiba et al. 2012, MSSE) — descriptions state the predicted W' floor.
  - Rate ladders: power follows the cube law P ∝ R³ at constant stroke shape
    (Dudhia, Physics of Ergometers; docs/fisica_ghost §6).
  - Technique sessions: 18–26 spm band of the Beta "technique" preset
    (docs/super_prompt §3.1); race prep at 30–36 spm ("race" preset).
Segments: (seconds, FTP fraction [-1 = free/rest], spm [0 = free]).
"""
import json, math, os, re, uuid

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
CP, W0 = 200.0, 12000.0          # reference athlete for W' predictions

def wprime_floor(segs, ftp=CP):
    bal = W0
    lo = W0
    for length, frac, _ in segs:
        p = 0.55 * ftp if frac < 0 else frac * ftp
        if p > ftp:
            bal = max(0.0, bal - (p - ftp) * length)
            lo = min(lo, bal)
        else:
            tau = 546.0 * math.exp(-0.01 * (ftp - p)) + 316.0
            bal = W0 - (W0 - bal) * math.exp(-length / tau)
    return int(round(100 * lo / W0))

def wu(mins=6, spm=20):  return [(mins * 60, 0.60, spm)]
def cd(mins=5):          return [(mins * 60, -1.0, 0)]

def xsr(title, desc, diff, tags, segs):
    return json.dumps({
        "MetaData": {"FileVersionNumber": 2, "Guid": str(uuid.uuid4()),
                     "IsC2Verified": False, "Tags": tags},
        "TrainingData": {"Category": "", "Title": title, "UnitType": 1,
            "Difficulty": diff, "Description": desc,
            "Schedule": [{"Length": s, "FTPTarget": f, "StrokesPerMin": r}
                         for s, f, r in segs],
            "Events": []},
        "EditorData": {"EventLinks": []}}, indent=2)

def zwo(title, desc, tags, segs):
    L = ['<?xml version="1.0" ?>', "<workout_file>",
         "  <author>SmartRower Pro</author>",
         f"  <name>{title.replace('&', '&amp;')}</name>",
         f"  <description>{desc.replace('&', '&amp;')}</description>",
         "  <sportType>rowing</sportType>", "  <tags>"]
    L += [f'    <tag name="{t}"/>' for t in tags] + ["  </tags>", "  <workout>"]
    for s, f, r in segs:
        if f < 0:
            L.append(f'    <FreeRide Duration="{s}" FlatRoad="1"/>')
        else:
            cad = f' Cadence="{r}"' if r else ""
            L.append(f'    <SteadyState Duration="{s}" Power="{f:.2f}"{cad}/>')
    L += ["  </workout>", "</workout_file>", ""]
    return "\n".join(L)

LIB = []   # (category, title, desc, difficulty, tags, segs)
def add(cat, title, desc, diff, tags, segs):
    LIB.append((cat, title, desc, diff, tags, segs))

# ── 1. Technique & Form — Beta "technique" preset band (18–26 spm) ──────────
for mins in (20, 25, 30, 40):
    for spm in (18, 20, 22, 24):
        add("Technique & Form", f"Beta Shape {mins}min @{spm}spm",
            f"Steady {mins} minutes at {spm} spm in the 'technique' preset band: "
            "chase the Beta(2,3) ghost — peak at 33% of the drive, fullness 0.56. "
            "Watch the Coach tab: one cue per stroke, sequence before shape.",
            1, ["TECHNIQUE", "FORM", "GHOST"],
            wu() + [(mins * 60, 0.65, spm)] + cd())
for reps, on in ((6, 3), (5, 4), (4, 5)):
    add("Technique & Form", f"Catch Sharpener {reps}x{on}min",
        f"{reps}x{on}min at 20 spm focusing on the catch factor: seat reverses "
        "15-35 ms before the handle (Kleshnev). 1min free between reps to reset.",
        1, ["TECHNIQUE", "CATCH"],
        wu() + sum([[(on * 60, 0.68, 20), (60, -1.0, 0)] for _ in range(reps)], []) + cd())
for a, b in ((18, 24), (20, 26)):
    add("Technique & Form", f"Form Hold {a}-{b}spm",
        f"Alternate 4min @{a} and 4min @{b} spm, 4 rounds, holding the same "
        "curve shape: fullness and peak position must not drift with the rate.",
        2, ["TECHNIQUE", "FORM"],
        wu() + sum([[(240, 0.65, a), (240, 0.72, b)] for _ in range(4)], []) + cd())
for i, spm in enumerate((19, 21, 23)):
    add("Technique & Form", f"Sequence Drill {i+1}",
        f"3x8min @{spm} spm. Legs-trunk-arms in order: the seat trace must lead "
        "the handle. RSF target 0.90 in the first 20% of the drive.",
        1, ["TECHNIQUE", "SEQUENCE"],
        wu() + sum([[(480, 0.66, spm), (90, -1.0, 0)] for _ in range(3)], []) + cd())
add("Technique & Form", "Ghost Mirror 30min",
    "30min at 20 spm rowing ON the ghost: force, seat and arm curves overlaid. "
    "Score >80 on shape RMSE for 10 strokes in a row before raising the rate.",
    1, ["TECHNIQUE", "GHOST"], wu() + [(1800, 0.65, 20)] + cd())
add("Technique & Form", "Slow Motion Row",
    "24min at 16-18 spm, exaggerated 1:3 drive-to-recovery. The slowest row is "
    "the hardest to do well: every flaw shows at low rate.",
    1, ["TECHNIQUE", "RATIO"], wu() + [(1440, 0.60, 17)] + cd())

# ── 2. Endurance — UT2/UT1 zones, TRIMP-steady ──────────────────────────────
for mins in (30, 40, 50, 60, 70):
    for frac, tag in ((0.62, "UT2"), (0.66, "UT2+"), (0.70, "UT1")):
        add("Endurance", f"{tag} Steady {mins}min",
            f"{mins} minutes at {int(frac*100)}% FTP ({tag}). Aerobic base: HR "
            "zone 2-3, decoupling under 5% — if HR drifts, the base isn't there yet.",
            1 if frac < 0.65 else 2, ["ENDURANCE", "AEROBIC", tag],
            wu() + [(mins * 60, frac, 20)] + cd())
for mins in (40, 50, 60):
    half = mins * 30
    add("Endurance", f"Negative Split {mins}min",
        f"First half at 62% FTP, second at 72%: finishing faster than you start "
        "is the oldest race lesson. Keep the stroke shape identical across halves.",
        2, ["ENDURANCE", "PACING"],
        wu() + [(half, 0.62, 20), (half, 0.72, 22)] + cd())
for reps in (3, 4):
    add("Endurance", f"Aerobic Waves {reps}x10min",
        f"{reps}x10min alternating 65/75% FTP in 2:30 blocks. Wave endurance: "
        "steady TRIMP accumulation without threshold stress.",
        2, ["ENDURANCE", "WAVES"],
        wu() + sum([[(150, 0.65, 20), (150, 0.75, 22)] * 2 + [(120, -1.0, 0)]
                    for _ in range(reps)], []) + cd())
add("Endurance", "Long Haul 80min",
    "The weekly monument: 80 minutes at 60% FTP, 18-20 spm. Fuel before, "
    "hydrate during, and let the decoupling metric grade your aerobic engine.",
    2, ["ENDURANCE", "LONG"], wu() + [(4800, 0.60, 19)] + cd())

# ── 3. Threshold & FTP ──────────────────────────────────────────────────────
for reps, on, rest in ((2, 20, 5), (3, 15, 4), (4, 10, 3), (5, 8, 2), (3, 12, 3), (6, 6, 2), (4, 12, 4)):
    add("Threshold", f"Threshold {reps}x{on}min",
        f"{reps}x{on}min at 96-100% FTP, {rest}min easy between. Classic FTP "
        "builder: the last rep should feel honest, not heroic.",
        3, ["THRESHOLD", "FTP"],
        wu(8) + sum([[(on * 60, 0.98, 24), (rest * 60, 0.50, 18)]
                     for _ in range(reps)], []) + cd())
for reps, over, under in ((4, 1.08, 0.92), (5, 1.05, 0.90), (6, 1.10, 0.88), (4, 1.06, 0.94), (3, 1.12, 0.85)):
    segs = wu(8) + sum([[(120, over, 26), (180, under, 22)] for _ in range(reps)], []) + cd()
    add("Threshold", f"Over-Under {reps}x(2+3)",
        f"{reps} rounds of 2min @{int(over*100)}% / 3min @{int(under*100)}% FTP. "
        f"Teaches lactate shuttling; predicted W' floor ≈ {wprime_floor(segs)}% "
        "(CP=FTP=200W, W'=12kJ).",
        3, ["THRESHOLD", "OVER-UNDER"], segs)
for mins in (20, 30, 40):
    add("Threshold", f"Sweet Spot {mins}min",
        f"{mins}min at 88-92% FTP, 22-24 spm: maximum adaptation per unit of "
        "fatigue. The workhorse between hard days.",
        2, ["THRESHOLD", "SWEETSPOT"], wu(8) + [(mins * 60, 0.90, 23)] + cd())
for i, (r1, r2) in enumerate(((0.95, 1.02), (0.92, 1.05))):
    add("Threshold", f"FTP Builder {i+1}",
        f"3x12min ramping {int(r1*100)}→{int(r2*100)}% FTP inside each rep. "
        "Finishing over threshold trains the lift a 2k demands.",
        3, ["THRESHOLD", "RAMP"],
        wu(8) + sum([[(360, r1, 23), (360, r2, 25), (180, 0.50, 18)]
                     for _ in range(3)], []) + cd())
add("Threshold", "Hour of Power",
    "60min at 85% FTP, 22 spm, no breaks. A mental monument as much as a "
    "physical one — TRIMP will tell you the truth tomorrow.",
    3, ["THRESHOLD", "LONG"], wu(8) + [(3600, 0.85, 22)] + cd())

# ── 4. W' Engineered — intervals designed on the Skiba equations ───────────
for reps, on, frac, rest in ((8, 60, 1.30, 120), (6, 90, 1.25, 150),
                             (5, 120, 1.20, 180), (10, 45, 1.35, 90),
                             (4, 180, 1.15, 240), (12, 30, 1.50, 90),
                             (6, 75, 1.28, 120), (5, 100, 1.22, 160),
                             (7, 60, 1.32, 100), (9, 50, 1.38, 100),
                             (4, 150, 1.18, 210), (8, 80, 1.24, 140),
                             (15, 30, 1.45, 60), (6, 110, 1.20, 170)):
    segs = wu(8) + sum([[(on, frac, 28), (rest, 0.45, 18)] for _ in range(reps)], []) + cd()
    floor = wprime_floor(segs)
    add("W-Prime Intervals", f"W' Burn {reps}x{on}s @{int(frac*100)}%",
        f"{reps}x{on}s at {int(frac*100)}% FTP, {rest}s recovery. Engineered on "
        f"the W'-balance model: predicted floor ≈ {floor}% of a 12kJ W' at "
        "CP=FTP=200W. Watch the battery card — abort a rep if it hits red early.",
        4 if floor < 30 else 3, ["WPRIME", "INTERVALS", "ANAEROBIC"], segs)
for reps, on in ((3, 240), (4, 200)):
    segs = wu(8) + sum([[(on, 1.10, 26), (on // 2, 0.50, 18)] for _ in range(reps)], []) + cd()
    add("W-Prime Intervals", f"Depletion Blocks {reps}x{on//60}min",
        f"{reps}x{on//60}min at 110% FTP with half-time recovery: long, shallow "
        f"W' drains (predicted floor ≈ {wprime_floor(segs)}%). VO2max territory.",
        4, ["WPRIME", "VO2MAX"], segs)
for i, (dur, frac) in enumerate(((300, 1.05), (420, 1.03))):
    segs = wu(8) + [(dur, frac, 26), (300, 0.45, 18), (dur, frac, 26)] + cd()
    add("W-Prime Intervals", f"Twin Peaks {dur//60}min",
        f"2x{dur//60}min just above CP at {int(frac*100)}% FTP. Slow-drip W' "
        f"depletion (floor ≈ {wprime_floor(segs)}%): learn what 'just above' feels like.",
        3, ["WPRIME", "CP"], segs)
segs = wu(8) + [(60, 1.60, 30), (240, 0.50, 18), (120, 1.30, 28),
                (240, 0.50, 18), (300, 1.08, 26)] + cd()
add("W-Prime Intervals", "The Staircase Down",
    "60s @160%, 2min @130%, 5min @108% FTP with 4min recoveries: three drains "
    f"of decreasing depth (predicted floor ≈ {wprime_floor(segs)}%). "
    "The recharge between reps IS the workout.",
    4, ["WPRIME", "MIXED"], segs)

# ── 5. Rate Ladders — cube law P ∝ R³ ───────────────────────────────────────
CUBE = lambda r, r0=20, p0=0.60: min(1.6, p0 * (r / r0) ** 3)
for r0, steps, hold in ((18, (18, 20, 22, 24), 180), (18, (18, 20, 22, 24, 26), 150),
                        (20, (20, 22, 24, 26), 180), (20, (20, 24, 28), 240),
                        (22, (22, 26, 30), 180), (18, (18, 22, 26, 30), 120),
                        (16, (16, 18, 20, 22), 210), (20, (20, 23, 26, 29), 150),
                        (24, (24, 27, 30), 150), (18, (18, 21, 24, 27, 30), 120)):
    segs = wu() + [(hold, round(CUBE(r, r0, 0.60), 2), r) for r in steps] + cd()
    add("Rate Ladders", f"Cube Ladder {steps[0]}-{steps[-1]}spm",
        f"Rate ladder {'-'.join(map(str, steps))} spm holding the SAME stroke: "
        "power targets follow the cube law P∝R³ — at constant drive length, "
        "rating up alone nearly doubles the watts. Do not let the shape change.",
        2, ["RATE", "CUBELAW"], segs)
for steps in ((24, 22, 20, 18), (28, 24, 20)):
    segs = wu() + [(180, round(CUBE(r, steps[-1], 0.60), 2), r) for r in steps] + cd()
    add("Rate Ladders", f"Descending Ladder {steps[0]}-{steps[-1]}spm",
        f"Down the ladder {'-'.join(map(str, steps))} spm: keep the per-stroke "
        "power as the rate falls — the stroke must get LONGER, not softer.",
        2, ["RATE", "CUBELAW"], segs)
for reps in (4, 6):
    add("Rate Ladders", f"Rate Surges {reps}x(3+1)",
        f"{reps} rounds: 3min @20 spm then 1min @28 spm at cube-law power. "
        "Surge without shortening: drive length is the thing being tested.",
        3, ["RATE", "SURGE"],
        wu() + sum([[(180, 0.60, 20), (60, round(CUBE(28, 20, 0.60), 2), 28)]
                    for _ in range(reps)], []) + cd())
for a, b, mins in ((20, 24, 24), (22, 26, 20)):
    add("Rate Ladders", f"Shifting Gears {a}/{b}spm {mins}min",
        f"{mins}min alternating 2min @{a} and 2min @{b} spm. Gear changes on "
        "the water are won by whoever keeps the curve identical.",
        2, ["RATE", "SHIFTS"],
        wu() + [(120, round(CUBE(r, a, 0.62), 2), r)
                for _ in range(mins // 4) for r in (a, b)] + cd())

# ── 6. Race Prep — "race" preset 30-36 spm ──────────────────────────────────
add("Race Prep", "2k Race Simulation",
    "The full distance at target pace, 30-34 spm, 'race' preset ghost "
    "(peak 40%, fullness 0.61). Fly-and-die is a choice, not an accident: "
    "even splits, sprint the last 250m.",
    5, ["RACE", "2K"], wu(10) + [(420, 1.05, 32)] + cd(8))
for reps, dist_s in ((4, 105), (6, 105), (8, 52), (5, 105), (10, 52), (3, 160), (2, 210)):
    label = "500m" if dist_s > 60 else "250m"
    add("Race Prep", f"Race Pieces {reps}x{label}",
        f"{reps}x{label} at 2k pace +2%, full recovery. Quality over volume: "
        "every piece at 30-34 spm on the race ghost, identical splits.",
        4, ["RACE", "PIECES"],
        wu(10) + sum([[(dist_s, 1.10, 32), (dist_s * 2, 0.40, 18)]
                      for _ in range(reps)], []) + cd())
add("Race Prep", "Start Sequence Practice",
    "10 racing starts: 5 strokes max, 3/4-1/2-3/4-full-full slide, then 40s "
    "paddle. The first five strokes decide your rhythm for the next 200.",
    4, ["RACE", "STARTS"],
    wu(10) + sum([[(15, 1.50, 38), (45, 0.40, 18)] for _ in range(10)], []) + cd())
for i, (front, back) in enumerate(((1.10, 1.00), (1.00, 1.10))):
    name = "Fast Start" if front > back else "Fast Finish"
    add("Race Prep", f"2k Split Strategy: {name}",
        f"Broken 2k as 4x500m: {'first' if front>back else 'last'} 500 at "
        f"{int(max(front,back)*100)}% of race power. Rehearse the plan, then race the plan.",
        4, ["RACE", "PACING"],
        wu(10) + [(105, front, 32), (105, 1.02, 31), (105, 1.02, 31),
                  (105, back, 33)] + cd())
add("Race Prep", "1k Time Trial",
    "All-out 1000m, open rate. Benchmark for CP/W' estimation: log it, the "
    "athlete card uses it.", 5, ["RACE", "TT"], wu(10) + [(210, 1.15, 33)] + cd(8))
add("Race Prep", "5k Tempo Test",
    "5000m at controlled 85-88% FTP, 24-26 spm. The aerobic anchor of race "
    "training — and the other data point your CP estimate needs.",
    3, ["RACE", "5K", "TEST"], wu(10) + [(1200, 0.87, 25)] + cd())

# ── 7. HIIT & Sprint ────────────────────────────────────────────────────────
for reps, on, off, frac in ((10, 30, 30, 1.40), (12, 30, 30, 1.35),
                            (10, 40, 20, 1.30), (16, 20, 10, 1.55),
                            (8, 20, 10, 1.70), (10, 60, 60, 1.25),
                            (14, 30, 15, 1.38), (12, 45, 15, 1.26),
                            (6, 90, 90, 1.22), (20, 15, 15, 1.60),
                            (8, 50, 25, 1.32), (10, 35, 25, 1.36)):
    segs = wu(8) + sum([[(on, frac, 30), (off, 0.40, 16)] for _ in range(reps)], []) + cd()
    add("HIIT", f"HIIT {reps}x{on}/{off}s",
        f"{reps} reps of {on}s @{int(frac*100)}% FTP with {off}s easy. "
        f"Predicted W' floor ≈ {wprime_floor(segs)}%. Short, sharp, honest.",
        4, ["HIIT", "ANAEROBIC"], segs)
add("HIIT", "Tabata Classic",
    "8x20s all-out / 10s off — the original protocol. Four minutes that "
    "empty the W' battery almost completely.",
    5, ["HIIT", "TABATA"],
    wu(8) + sum([[(20, 1.70, 34), (10, 0.30, 0)] for _ in range(8)], []) + cd())
for sets in (2, 3):
    add("HIIT", f"Tabata x{sets}",
        f"{sets} Tabata blocks (8x20/10) with 4min full recovery between: "
        "the W' recharge between sets is what makes set 2 possible at all.",
        5, ["HIIT", "TABATA"],
        wu(8) + sum([sum([[(20, 1.65, 34), (10, 0.30, 0)] for _ in range(8)], [])
                     + [(240, -1.0, 0)] for _ in range(sets)], []) + cd())
for reps in (6, 8, 10):
    add("HIIT", f"Sprint Repeats {reps}x15s",
        f"{reps}x15s maximal sprints, 105s recovery: pure power, full recharge. "
        "Peak-force card should hit its best numbers of the week here.",
        4, ["SPRINT", "POWER"],
        wu(8) + sum([[(15, 1.80, 36), (105, 0.40, 16)] for _ in range(reps)], []) + cd())

# ── 8. Recovery ─────────────────────────────────────────────────────────────
for mins in (15, 20, 25, 30, 35, 40, 45):
    add("Recovery", f"Regeneration {mins}min",
        f"{mins} easy minutes at 50-55% FTP, 17-18 spm, HR zone 1. Recovery is "
        "training: TRIMP stays low, technique stays honest.",
        0, ["RECOVERY", "ZONE1"], [(mins * 60, 0.52, 18)])
for i, spm in enumerate((16, 18, 20)):
    add("Recovery", f"Recovery Flow {i+1}",
        f"25min at {spm} spm alternating 4min rowing / 1min free paddle. "
        "Blood flow without load — the day after a W' session.",
        0, ["RECOVERY", "FLOW"],
        sum([[(240, 0.50, spm), (60, -1.0, 0)] for _ in range(5)], []))


# ── extra drills & distance rows ────────────────────────────────────────────
for pause, mins in (("finish", 16), ("finish", 24), ("half-slide", 16), ("half-slide", 24)):
    add("Technique & Form", f"Pause Drill {pause.title()} {mins}min",
        f"{mins}min at 18 spm with a 1s pause at the {pause}: the pause exposes "
        "balance and posture errors that continuous rowing hides.",
        1, ["TECHNIQUE", "PAUSE"], wu() + [(mins * 60, 0.58, 18)] + cd())
for mins in (16, 24, 32):
    add("Technique & Form", f"Connection Row {mins}min",
        f"{mins}min at 19 spm, maximum attention to the first 20% of the drive: "
        "hang on the handle, RSF at 0.90, no early back opening.",
        1, ["TECHNIQUE", "CONNECTION"], wu() + [(mins * 60, 0.62, 19)] + cd())
for km, mins in ((5, 24), (6, 28), (8, 37), (10, 47), (12, 56)):
    add("Endurance", f"Distance Row {km}k",
        f"{km},000 metres continuous at 65% FTP, 19-21 spm. Distance monuments "
        "build the aerobic base — and the History tab's trend line.",
        2, ["ENDURANCE", "DISTANCE"], wu() + [(mins * 60, 0.65, 20)] + cd())

def main():
    safe = lambda s: re.sub(r"[^A-Za-z0-9]+", "", s.title())
    nx = nz = 0
    for cat, title, desc, diff, tags, segs in LIB:
        base = safe(title)
        dx = os.path.join(OUT, "xsr_format", "Workouts", cat)
        dz = os.path.join(OUT, "zwo_format", "Workouts", cat)
        os.makedirs(dx, exist_ok=True); os.makedirs(dz, exist_ok=True)
        with open(os.path.join(dx, base + ".xsr"), "w", encoding="utf-8") as f:
            f.write(xsr(title, desc, diff, tags, segs)); nx += 1
        with open(os.path.join(dz, base + ".zwo"), "w", encoding="utf-8") as f:
            f.write(zwo(title, desc, tags, segs)); nz += 1
    total = lambda segs: sum(s for s, _, _ in segs)
    mins = sum(total(s) for *_, s in LIB) / 60
    print(f"{nx} .xsr + {nz} .zwo written to {OUT} "
          f"({len(LIB)} workouts, {mins:.0f} training minutes)")

if __name__ == "__main__":
    main()
