"""Shot 5.1: the Purple Death comes down in canisters that open in the air (owner, 2026-11-21: "not a bomb").

Many blunt purple canisters (blender/props/props_build.py build_pdcan_*: no fins, no nose cone, glowing windows) tumble
down through the night cloud at different distances. One after another each bursts: the nose and tail caps blow off
along its axis, the two half shells peel apart and tumble away, the core of glowing ampoules is bared and the aerosol
balloons out of it (M_Intro_PDGas spheres that grow and thin out) until the purple gas has become cloud.

All canister parts share one origin (the canister centre, long axis X), so every part's world transform is the
canister's transform composed with the part's own opening motion. Rotations are composed as axes (UE rotator
convention) and turned back into rotators with the same formula FMatrix::Rotator uses (intro_berlin).
"""
import math
import random

import intro_berlin as berlin

ROOT = "/Game/RTS_Survival/Cinematics/Intro1976/"
SHELL = ROOT + "Props/SM_Intro_PDCanShell"
NOSE = ROOT + "Props/SM_Intro_PDCanNose"
TAIL = ROOT + "Props/SM_Intro_PDCanTail"
CORE = ROOT + "Props/SM_Intro_PDCanCore"
SPHERE = "/Engine/BasicShapes/Sphere"
GAS = ROOT + "Materials/MI_Intro_PDGas_%d"
CORE_GLOW = ROOT + "Materials/MI_Intro_PDGlowCore"      # the bared ampoules: dimmer than the windows (a wide area)

# (start position, velocity cm/s, pitch, yaw, roll rate deg/s, open at (fraction of the shot), size, puff radius cm)
# The camera sits at the set origin looking along +X (fov 55): half the frame width at distance d is about 0.52 d.
CANISTERS = [
    ((900.0, -250.0, 360.0), (-6.0, 40.0, -85.0), -62.0, 28.0, 34.0, 0.42, 1.0, 300.0),     # hero, 9 m
    ((2300.0, 760.0, 820.0), (0.0, -20.0, -120.0), -70.0, 160.0, -40.0, 0.24, 1.0, 1300.0),
    ((3300.0, -1250.0, 900.0), (0.0, 30.0, -140.0), -55.0, -30.0, 45.0, 0.33, 1.0, 1700.0),
    ((4600.0, 1700.0, 1500.0), (0.0, -15.0, -150.0), -75.0, 80.0, 30.0, 0.15, 1.0, 2200.0),
    ((1700.0, -700.0, -150.0), (0.0, 20.0, -90.0), -66.0, -110.0, -28.0, 0.58, 1.0, 900.0),
    ((6200.0, -2600.0, 1900.0), (0.0, 40.0, -160.0), -60.0, 15.0, 36.0, 0.5, 1.0, 2800.0),
    ((5400.0, 300.0, 700.0), (0.0, -10.0, -150.0), -72.0, 200.0, -30.0, 0.68, 1.0, 2400.0),
    ((7600.0, 2900.0, 1100.0), (0.0, -30.0, -170.0), -58.0, 130.0, 40.0, 0.36, 1.0, 3200.0),
    ((2900.0, 1500.0, -300.0), (0.0, -25.0, -110.0), -64.0, 60.0, -38.0, 0.77, 1.0, 1400.0),
]


def _axes_mul(parent, child):
    """World axes of a child given in its parent's frame (each child axis as parent-frame coordinates)."""
    return tuple(tuple(c[0] * parent[0][i] + c[1] * parent[1][i] + c[2] * parent[2][i] for i in range(3)) for c in child)


def _local_axes(roll_deg=0.0, yaw_deg=0.0, pitch_deg=0.0):
    return berlin._axes_from_rotator(pitch_deg, yaw_deg, roll_deg)


def _canister_state(spec, t):
    """Location and axes of a canister's centre at t seconds (it tumbles about its long axis and wobbles)."""
    start, velocity, pitch, yaw, roll_rate = spec[0], spec[1], spec[2], spec[3], spec[4]
    location = tuple(start[i] + velocity[i] * t for i in range(3))
    axes = berlin._axes_from_rotator(pitch + 6.0 * math.sin(t * 0.9 + yaw), yaw + 8.0 * math.sin(t * 0.6), roll_rate * t)
    return location, axes


def _place(location, axes, offset, local):
    world = tuple(location[i] + offset[0] * axes[0][i] + offset[1] * axes[1][i] + offset[2] * axes[2][i] for i in range(3))
    return world, berlin._rotator_from_axes(*_axes_mul(axes, local))


def _opening(dt):
    """0 before the burst, then a fast blast that eases into a drift."""
    if dt <= 0.0:
        return 0.0
    return 1.0 - math.exp(-dt * 3.2)


def canister_actors(at, seconds):
    """at(set, x, y, z) -> world point; seconds: the shot length."""
    actors = []
    rng = random.Random(1945)
    steps = int(seconds * 12)
    for index, spec in enumerate(CANISTERS):
        open_at = spec[5] * seconds
        scale = spec[6]
        parts = {"Core": [], "Nose": [], "Tail": [], "ShellA": [], "ShellB": []}
        previous = {}
        for k in range(steps + 1):
            t = seconds * k / steps
            dt = t - open_at
            sep = _opening(dt)
            drift = max(0.0, dt)
            location, axes = _canister_state(spec, t)
            moves = {
                "Core": ((0.0, 0.0, 0.0), _local_axes()),
                "Nose": ((160.0 * sep + 45.0 * drift, 0.0, 0.0), _local_axes(roll_deg=60.0 * sep, yaw_deg=35.0 * sep)),
                "Tail": ((-150.0 * sep - 40.0 * drift, 0.0, 0.0), _local_axes(roll_deg=-50.0 * sep, pitch_deg=40.0 * sep)),
            }
            for name, side in (("ShellA", 0.0), ("ShellB", 180.0)):
                frame = _local_axes(roll_deg=side)
                peel = _axes_mul(frame, _local_axes(roll_deg=70.0 * sep + 30.0 * drift, yaw_deg=28.0 * sep))
                out = 95.0 * sep + 35.0 * drift
                direction = frame[1]
                moves[name] = ((direction[0] * out, direction[1] * out, direction[2] * out), peel)
            for name, (offset, local) in moves.items():
                world, rotator = _place(location, axes, tuple(o * scale for o in offset), local)
                rotator = berlin._unwrap(previous.get(name), rotator)
                previous[name] = rotator
                parts[name].append((k / float(steps), at("Intro_Clouds", *world), rotator, scale))
        for name, asset in (("Core", CORE), ("Nose", NOSE), ("Tail", TAIL), ("ShellA", SHELL), ("ShellB", SHELL)):
            actors.append({"kind": "mesh", "asset": asset, "label": "Canister%d_%s" % (index, name), "keys": parts[name],
                           "shadow": False, "mats": [None, CORE_GLOW] if name == "Core" else None})
        # The aerosol: puffs born at the bared core that balloon out and thin into cloud.
        burst, _ = _canister_state(spec, open_at)
        radius = spec[7]
        for puff in range(3):
            angle = rng.uniform(0.0, 2.0 * math.pi)
            lift = rng.uniform(-0.3, 0.5)
            spread = (math.cos(angle) * 0.6, math.sin(angle), lift)
            size = radius * rng.uniform(0.6, 1.0)
            keys = []
            for f in (0.0, 0.08, 0.25, 0.55, 1.0):
                t = open_at + (seconds - open_at) * f
                grow = (1.0 - math.exp(-(t - open_at) * 1.6)) if t > open_at else 0.0
                r = 25.0 + size * grow
                centre, _ = _canister_state(spec, open_at + (t - open_at) * 0.35)
                p = tuple(centre[i] + spread[i] * r * 0.55 for i in range(3))
                keys.append((spec[5] + (1.0 - spec[5]) * f, at("Intro_Clouds", *p), (0, rng.uniform(0, 360), 0), r / 50.0))
            actors.append({"kind": "mesh", "asset": SPHERE, "label": "Gas%d_%d" % (index, puff), "mats": GAS % ((index + puff) % 4),
                           "shadow": False, "spawn": (spec[5], 1.0), "keys": keys})
    hero_open = CANISTERS[0][5] * seconds
    hero_at, _ = _canister_state(CANISTERS[0], hero_open)
    actors.append({"kind": "point", "label": "HeroBurst", "intensity": 5.0, "color": (0.62, 0.2, 1.0, 1.0), "radius": 2500.0,
                   "spawn": (CANISTERS[0][5], 1.0), "keys": [(0.0, at("Intro_Clouds", *hero_at), (0, 0, 0))]})
    # A cold key on the hero canister's flank, so the dark casing reads against the cloud before it opens.
    start, _ = _canister_state(CANISTERS[0], 0.0)
    end, _ = _canister_state(CANISTERS[0], seconds)
    actors.append({"kind": "point", "label": "CanisterKey", "intensity": 26.0, "color": (0.75, 0.8, 1.0, 1.0), "radius": 1200.0,
                   "keys": [(0.0, at("Intro_Clouds", start[0] - 300, start[1] - 150, start[2] + 80), (0, 0, 0)),
                            (1.0, at("Intro_Clouds", end[0] - 300, end[1] - 150, end[2] + 80), (0, 0, 0))]})
    return actors
