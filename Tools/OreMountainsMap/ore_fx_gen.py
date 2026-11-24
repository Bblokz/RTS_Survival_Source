"""Ambient sand and dust effects for Ger_2ndOreMountains: tuned presets of the Fab "DustAndSandStorm" systems.

    python Source/Tools/OreMountainsMap/ore_fx_gen.py      (after ore_terrain_gen.py and the cliff layout)

Writes Saved/OreMountains/terrain/Ore_fx.json: {"presets": {...}, "items": [[preset, label, x, y, z_mode, z, yaw], ...]}
z_mode "ground" = z is a height above the landscape, "abs" = z is a world height.

The pack's systems are left untouched. Out of the box almost none of them reads from the RTS camera (40 m up): they
are sized for a walking camera, dark grey-brown, and several need longer than a glance to build up. Each preset below
is a pack system plus user-parameter overrides and a scale, tested in game on this map (ore_fx_presets.py):
  SandStreak   NS_SandPush       wind-driven sand hugging the ground; the default is a near-black blob, 4000 GPU
                                  particles a second and 4 m long. AddWhiteColor / Emessive under about 0.4 leave
                                  the sand sprites dark grey on this ground (tried 0.22 / 0.28: a dark smudge)
  DustCloud    NS_StormSmoke_02  a wide soft dust haze drifting over open ground
"""
import json
import math
import os

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(ROOT, "Saved", "OreMountains", "terrain")
layout = json.load(open(os.path.join(OUT, "Ore_layout.json")))
B = layout["bounds"]
rng = np.random.default_rng(11)
PACK = "/Game/DustAndSandStorm/SandStorm/Niagara/"
WIND_YAW = 35.0                 # URTSAmbientFxSettings::WindYawDegrees: the dust devils and refinery smoke lean the same way
LIGHT = (0.80, 0.71, 0.55, 1.0)  # the map's pale dust (linear)
MID = (0.62, 0.54, 0.40, 1.0)
DARK = (0.45, 0.38, 0.27, 1.0)

PRESETS = {
    # 2026-11-13 second round: the owner asked for stronger effects. Denser and larger; the colour stays warm because
    # AddWhiteColor stays at 0.38 (0.2-0.3 with more Emessive came out whiter and cooler in the test sheet).
    # 2026-11-16 masked rework (ore_sandstreak_build.py): NS_SandPush now draws masked materials and its four SpawnRate
    # inputs are fixed literals in the system (mesh 40, smoke A 10, smoke B 6, grains 400 a second), so the four
    # *SpawnRate overrides below no longer do anything; the per-MI Boost makes up for the lower counts.
    "SandStreak": {"system": PACK + "NS_SandPush", "scale": 1.0,
                   "floats": {"SandScale": 1.8, "Length": 2400.0, "AddWhiteColor": 0.38, "Emessive": 0.42,
                              "ParticleSmokeSizeScale": 2.6, "ParticleSpawnRate": 2000.0, "Sand_SpawnRate": 110.0,
                              "SmokeSpawnRate": 35.0, "SandSmokeSpawnRate": 35.0},
                   "colors": {"Color Maximum": (0.85, 0.70, 0.48, 1.0), "Color Minimum": (0.66, 0.54, 0.37, 1.0)}},
    "DustCloud": {"system": PACK + "NS_StormSmoke_02", "scale": 1.0,
                  "floats": {"Smoke_Size_Scale": 3.2, "Smoke_SpawnRate_Scale": 1.3, "Emessive_Scale": 1.6},
                  "colors": {"Smoke_Color_A": (0.86, 0.76, 0.58, 0.6), "Smoke_Color_B": (0.62, 0.53, 0.38, 0.7)}},
}

items = []


def add(preset, label, x, y, z_mode, z, yaw):
    items.append([preset, label, float(x), float(y), z_mode, float(z), float(yaw)])


# sand streaks on open ground, along the wind
STREAKS = [(2500, -12000), (-2500, 9000), (3000, 16500), (-3500, -17000), (1500, -22000),      # valley floor
           (-15500, -4000), (-17500, 14500), (-19500, -14000),                                # the bench
           (9000, 6500), (15000, -17000), (21000, 3000)]                                      # the foothills
STREAKS += [(-1500, -6500), (-4500, -9500), (2500, 5500), (4500, 22000),                       # more on the valley floor
            (-15000, 5000), (-18500, -22000), (-20500, 21000),                                 # the bench
            (12500, -2500), (17000, 17500), (22000, -12000)]                                   # the foothills
for k, (x, y) in enumerate(STREAKS):
    add("SandStreak", "Streak_%02d" % k, x, y, "ground", 10.0, WIND_YAW + float(rng.uniform(-12, 12)))

# dust clouds where the valley opens to the map edges and on the foothill slope
for k, (x, y) in enumerate([(0, -25500), (1500, 26000), (22000, 10000), (-4000, 0), (9000, -21000), (-6000, 21500)]):
    add("DustCloud", "Cloud_%d" % k, x, y, "ground", 10.0, WIND_YAW)

# Tried and dropped (2026-11-13): NS_SandFall01/02/04/05 on the rock lips and NS_FallingDust_01 under the viaduct
# stubs. Scaled up to 2.5-3 and recoloured they still do not show from the RTS camera at any time after spawning
# (time series at 3, 8, 16 and 28 s), so placing them would only cost simulation time.

json.dump({"presets": PRESETS, "items": items}, open(os.path.join(OUT, "Ore_fx.json"), "w"), indent=1)
counts = {}
for item in items:
    counts[item[0]] = counts.get(item[0], 0) + 1
print("fx items", counts)
