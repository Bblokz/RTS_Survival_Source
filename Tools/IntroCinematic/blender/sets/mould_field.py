"""The Purple Death's vessel field on the trench mould mats, shared by the mesh and the engine material.

The mats' UVs are surface coordinates in metres (U = s + 0.5 + offset, V = t + 0.5 + offset). build_mould_stages.py
raises the vessels and nodules out of the subdivided mats from this field, and bake_mould_relief.py bakes the same
field into T_Intro_MouldRelief_ORM (R = vessels, G = nodules) over the UV domain below, which M_Intro_PurpleDeath
samples on the mats, so its colour and glow sit exactly on the raised vessels. No bpy here (the bake is plain python).
"""
import numpy as np

# UV domain of the relief texture (Blender UV units = metres): U in [-4, 12], V in [-4, 4]
U0, U_SPAN, V0, V_SPAN = -4.0, 16.0, -4.0, 8.0
# per-mat field offset (also added to that mat's UVs), so the three mats do not share one vessel pattern
OFFSETS = {"SM_Intro_Mould_Mat_A": (0.0, 0.0), "SM_Intro_Mould_Mat_B": (5.0, 0.0), "SM_Intro_Mould_Mat_C": (9.0, 0.0)}


def sn(x, y, seed, freq, octaves=3, gain=0.5):
    """Smooth analytic noise (sum of random plane waves), ~unit std (same as sets_common.sn)."""
    r = np.random.default_rng(seed)
    out, amp, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        for _ in range(5):
            a = r.uniform(0, 2 * np.pi)
            f = freq * (2 ** o) * r.uniform(0.7, 1.3)
            ph = r.uniform(0, 2 * np.pi)
            out = out + amp * np.sin(2 * np.pi * f * (x * np.cos(a) + y * np.sin(a)) + ph)
        tot += amp * amp * 2.5
        amp *= gain
    return out / np.sqrt(tot)


def vessels(s, t):
    """0..1: thick winding vessels (~5 cm) along the zero contours of one noise, finer ones (~1.5 cm) of another,
    the fine ones only where a third noise lets them (so they cluster and branch off the big ones)."""
    a = 1.0 - np.abs(sn(s, t, 931, 0.9, 3))
    b = 1.0 - np.abs(sn(s + 3.1, t - 1.7, 932, 2.2, 2))
    c = 1.0 - np.abs(sn(s - 1.3, t + 2.9, 935, 4.6, 2))
    keep = np.clip((sn(s, t, 933, 0.6, 2) + 0.7) * 2.0, 0.0, 1.0)
    main = np.clip((a - 0.84) / 0.16, 0.0, 1.0) ** 1.5
    fine = np.clip((b - 0.88) / 0.12, 0.0, 1.0) ** 1.5 * 0.7 * keep
    capillary = np.clip((c - 0.92) / 0.08, 0.0, 1.0) ** 1.5 * 0.4 * keep   # a third, finer tier
    return np.maximum(np.maximum(main, fine), capillary)


def nodules(s, t):
    """0..1: swollen nodules scattered over the flesh."""
    return np.clip((sn(s, t, 934, 4.0, 2) - 1.0) / 0.8, 0.0, 1.0)
