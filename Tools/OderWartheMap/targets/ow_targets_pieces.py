"""Concept pieces for the Oder-Warthe Werk targets (see ow_targets_parts.py for conventions).

Groups (key -> builder(destroyed) -> Builder, or a portal deformer for the tunnel mouth):
  TUNNEL  TM_A..TM_F   collapsed tunnel mouth looks (the intact state is SM_OW_TunnelPortal)
  COMMAND CE_A..CE_F   command bunker entrance (pivot at the Werkeingang door, +X out of the door)
  JAMMER  JT_A..JT_F   jammer transmitter (pivot at the mast foot, the mast stays)
  RAIL    RE_A..RE_E   tunnel railway entrance (pivot at the tunnel portal pivot, the portal stays)
"""
import math

from mathutils import Vector

import ow_targets_parts as P
from ow_targets_parts import Builder, rng
from ow_targets_info import CE_INFO, JT_INFO, RE_INFO, TM_INFO  # noqa: F401

MATS = ['MI_OW_Concrete', 'MI_OW_ConcreteDark', 'MI_OW_Earth', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_Void',
        'MI_OW_Wood', 'MI_OW_CamoNet', 'MI_OW_Sandbag', 'MI_OW_Sod', 'MI_OW_Burnt']

# Portal numbers (ow_build.py build_portal, Blender frame)
OPEN_W, OPEN_H = 475.0, 520.0
HW_X0, HW_X1 = 240.0, 300.0
BACK_X = -350.0
# Mast: anchors at 90/210/330 deg, 6.4 m out; the clear sector used here is centred on -Y (270 deg).
SECTOR = -1.0


# ================================================================================================ tunnel mouth
def _lintel_sag(p, depth, half=OPEN_W + 160, side=0):
    if HW_X0 - 20 <= p.x <= HW_X1 + 30 or (BACK_X <= p.x <= HW_X1 and abs(p.y) <= OPEN_W + 5):
        y = p.y - side * half * 0.5
        k = max(0.0, 1.0 - (y / half) ** 2)
        if p.z > OPEN_H - 80 and abs(p.y) < half + (half * 0.5 if side else 0):
            return Vector((depth * 0.12 * k, 0, -depth * k))
    return Vector((0, 0, 0))


def _berm_crater(p, depth, cx=-150.0, r=520.0):
    if p.x < HW_X0 and p.z > 300:
        d = math.hypot(p.x - cx, p.y) / r
        if d < 1:
            return Vector((0, 0, -depth * (1 - P.ss(0, 1, d))))
    return Vector((0, 0, 0))


def _is_leaf(p):
    return p.x > HW_X1 + 4 and p.z > 22 and abs(p.y) < 745


def _leaf_fallen(p):
    s = 1 if p.y > 0 else -1
    return Vector((p.z * 0.85, -s * p.z * 0.1, -p.z * 0.93 + 18))


def tm_deform(key):
    """Vertex offset function for the imported portal mesh."""
    r = rng(91)
    nz = P.hnoise(17)

    def A(p):     # lintel drop
        return _lintel_sag(p, 260) + (_leaf_fallen(p) * 0.25 if _is_leaf(p) and p.y < 0 else Vector())

    def B(p):     # berm slide
        if p.x < HW_X1 + 40 and p.z > 380 and abs(p.y) < 820:
            f = (1 - P.ss(300, 820, abs(p.y))) * P.ss(380, 780, p.z)
            return Vector((260 * f, 0, -260 * f))
        return Vector()

    def C(p):     # headwall blown out
        if HW_X0 - 15 <= p.x <= HW_X1 + 30 and abs(p.y) < OPEN_W + 260 and p.z > 120:
            near = 1 - P.ss(OPEN_W - 40, OPEN_W + 260, abs(p.y)) if p.z < OPEN_H else 1 - P.ss(OPEN_H, OPEN_H + 200, p.z)
            return Vector((40 * near * nz(p.y, p.z), 0, -90 * near - 50 * near * nz(p.z, p.y)))
        return _lintel_sag(p, 140)

    def D(p):     # headwall tilted forward + berm crater
        o = _berm_crater(p, 300)
        if p.x >= HW_X0 - 15 and abs(p.y) <= 780 and not _is_leaf(p):
            th = math.radians(9)
            return o + Vector((p.z * math.sin(th), 0, p.z * (math.cos(th) - 1)))
        return o

    def E(p):     # doors torn off and down
        return _leaf_fallen(p) if _is_leaf(p) else _lintel_sag(p, 90)

    def F(p):     # half collapse (-Y side)
        return _lintel_sag(p, 330, side=-1)

    def G(p):     # the whole tunnel: roof caved along its length, headwall + lintel down, doors crushed
        if _is_leaf(p):
            return _leaf_fallen(p) + Vector((0, 0, -10))
        o = Vector()
        if p.x < HW_X0 + 5 and p.z > 250:                       # berm over the tunnel: a trough along X
            across = 1 - P.ss(0.0, 1.0, abs(p.y) / 720.0)
            along = 1 - 0.5 * P.ss(850.0, 1100.0, -p.x)
            o.z -= (430 + 60 * nz(p.x, p.y)) * across * along
        if HW_X0 - 20 <= p.x <= HW_X1 + 40 and abs(p.y) < 800:   # headwall: lintel and coping broken down
            k = 1 - P.ss(OPEN_W - 60, 800, abs(p.y))
            if p.z > OPEN_H - 90:
                o += Vector((70 * k, 0, -(380 + 40 * nz(p.y, 3.0)) * k))
            elif p.z > 200:
                o += Vector((25 * k * nz(p.y, p.z), 0, -60 * k))
        if BACK_X - 5 <= p.x < HW_X0 - 20 and abs(p.y) <= OPEN_W + 5 and p.z > 60:   # lining crushed flat
            o.z -= p.z * 0.75
        if p.x > HW_X1 + 30 and abs(p.y) > 700 and p.z > 250:   # wing wall tops crumbled
            o.z -= 110 * (0.6 + 0.4 * nz(p.x, p.y))
        return o
    return dict(TM_A=A, TM_B=B, TM_C=C, TM_D=D, TM_E=E, TM_F=F, TM_G=G)[key]


def tm_rubble(key):
    b = Builder(key + '_Rubble', MATS)
    if key == 'TM_A':
        P.rubble_heap(b, 3, 330, 0, 560, 520, 380, n=36)
        P.rubble_heap(b, 4, 120, 0, 260, 470, 470, n=12)
        b.obox((420, 40, 260), (180, 900, 90), 6, 'MI_OW_Concrete', roll_deg=8, pitch_deg=-30)
    elif key == 'TM_B':
        P.rubble_heap(b, 5, 420, 0, 760, 620, 470, n=22, earth='MI_OW_Earth')
        P.rubble_heap(b, 6, 150, 0, 260, 470, 520, n=6)
    elif key == 'TM_C':
        P.rubble_heap(b, 7, 380, 0, 640, 680, 300, n=48, size=(60, 220))
        P.rubble_heap(b, 8, 150, 0, 230, 470, 430, n=10)
        P.burn_scar(b, 9, 520, 0, 520, 620)
    elif key == 'TM_D':
        P.rubble_heap(b, 10, 380, 0, 520, 600, 420, n=26)
        b.obox((460, 0, 300), (90, 1000, 420), 0, 'MI_OW_Concrete', pitch_deg=-38)
        P.rubble_heap(b, 11, 120, 0, 240, 470, 500, n=8)
    elif key == 'TM_E':
        P.rubble_heap(b, 12, 300, 0, 380, 520, 520, n=60, size=(70, 200))
        P.burn_scar(b, 13, 650, 0, 480, 520)
    elif key == 'TM_F':
        P.rubble_heap(b, 14, 330, -260, 520, 380, 470, n=30)
        P.rubble_heap(b, 15, 340, 230, 380, 300, 170, n=14)
        P.rubble_heap(b, 16, 120, -150, 230, 330, 480, n=8)
    elif key == 'TM_G':
        P.rubble_heap(b, 17, 420, 0, 700, 760, 470, n=64, size=(60, 220))
        P.rubble_heap(b, 18, 140, 0, 260, 520, 560, n=14)
        b.obox((470, -60, 300), (170, 980, 95), 4, 'MI_OW_Concrete', roll_deg=-9, pitch_deg=-34)
        r = rng(19)
        for _ in range(14):                                      # roof slabs in the trough
            x, y = r.uniform(-950, 150), r.uniform(-330, 330)
            b.obox((x, y, 360 + r.uniform(-30, 30)), (r.uniform(160, 320), r.uniform(120, 260), 45),
                   r.uniform(0, 180), r.choice(('MI_OW_Concrete', 'MI_OW_ConcreteDark')),
                   pitch_deg=r.uniform(-30, 30), roll_deg=r.uniform(-30, 30))
        P.chunks(b, rng(20), 26, -400, 0, 600, 330, lambda x, y: 350, size=(40, 140))
    return b




# ================================================================================================ command entrance
def ce_A(d):      # Vorbau porch (gas lock)
    b = Builder('CE_A', MATS)
    if not d:
        b.box((0, -260, 0), (380, -220, 290), 'MI_OW_Concrete')
        b.box((0, 220, 0), (380, 260, 290), 'MI_OW_Concrete')
        b.box((340, -220, 0), (380, 220, 290), 'MI_OW_Concrete')
        b.box((-20, -290, 290), (410, 290, 335), 'MI_OW_Concrete')
        P.void_quad(b, [(381, -60, 150), (381, 60, 150), (381, 60, 175), (381, -60, 175)])
        P.mound(b, 190, 0, 260, 330, 420, 'MI_OW_Sod', seed=21, base=300, plateau=0.4)
        b.box((60, 255, 0), (200, 262, 210), 'MI_OW_DarkSteel')
        P.sandbags(b, 22, (40, 420), (260, 420), 130)
        P.net_on_poles(b, 23, 420, -340, 620, 340, 250, sag=20)
    else:
        b.box((0, -260, 0), (380, -220, 180), 'MI_OW_Concrete')
        b.box((0, 220, 0), (240, 260, 260), 'MI_OW_Concrete')
        b.obox((230, 0, 170), (430, 560, 45), 0, 'MI_OW_Concrete', pitch_deg=14, roll_deg=-5)
        hf = P.rubble_heap(b, 24, 330, 0, 330, 380, 190, n=26)
        P.chunks(b, rng(25), 10, 200, 0, 300, 300, hf)
        b.obox((470, 120, 10), (12, 140, 210), 80, 'MI_OW_DarkSteel', pitch_deg=88)
        P.sandbags(b, 26, (40, 420), (260, 420), 130, scatter=0.5)
        P.burn_scar(b, 27, 420, 0, 380, 420)
    return b


def ce_B(d):      # chicane + net + armoured door insert
    b = Builder('CE_B', MATS)
    if not d:
        P.door_plate(b, 8, 0, 0, 160, 230, 0)
        b.box((2, -95, 0), (14, 95, 240), 'MI_OW_DarkSteel', skip=('-x',))
    else:
        P.door_plate(b, 300, -40, 0, 160, 230, 30, pitch=86)
    sc = 0.55 if d else 0.0
    P.sandbags(b, 31, (170, -280), (170, 90), 140, scatter=sc)
    P.sandbags(b, 32, (330, -90), (330, 280), 140, scatter=sc)
    P.sandbags(b, 33, (170, -280), (330, -280), 140, scatter=sc)
    P.net_on_poles(b, 34, 30, -380, 620, 380, 270, destroyed=d, sag=30)
    if d:
        P.burn_scar(b, 35, 200, 0, 360, 380)
    return b


def ce_C(d):      # timbered trench approach
    b = Builder('CE_C', MATS)
    r = rng(41)
    segs = [((0, 0), (420, 0)), ((420, 0), (520, 150)), ((520, 150), (980, 150))]
    for k, (a, c) in enumerate(segs):
        a, c = Vector((a[0], a[1], 0)), Vector((c[0], c[1], 0))
        dv = c - a
        n = Vector((-dv.y, dv.x, 0)).normalized()
        mid = (a + c) / 2
        for s in (1, -1):
            if d and r.random() < 0.6:
                P.rubble_heap(b, 42 + k * 2 + (s > 0), mid.x + n.x * 40 * s, mid.y + n.y * 40 * s, dv.length / 2,
                              110, 110, n=4, yaw=math.degrees(math.atan2(dv.y, dv.x)))
                continue
            P.mound(b, mid.x + n.x * 190 * s, mid.y + n.y * 190 * s, dv.length / 2 + 60, 120, 130, 'MI_OW_Earth',
                    seed=50 + k, yaw=math.degrees(math.atan2(dv.y, dv.x)))
            steps = int(dv.length / 45)
            for i in range(steps + 1):
                p = a + dv * (i / steps) + n * 85 * s
                b.obox((p.x, p.y, 60), (14, 14, 140), 0, 'MI_OW_Wood', pitch_deg=r.uniform(-3, 3))
            b.beam(a + n * 85 * s + Vector((0, 0, 120)), c + n * 85 * s + Vector((0, 0, 120)), 6, 'MI_OW_Wood')
    if not d:
        P.door_plate(b, 8, 0, 0, 150, 220, 0)
        P.net(b, 47, 20, -190, 1000, 340, 175, sag=15)
    else:
        P.void_quad(b, [(6, -75, 0), (6, 75, 0), (6, 75, 220), (6, -75, 220)])
        P.burn_scar(b, 48, 400, 60, 500, 260)
    return b


def ce_D(d):      # free-standing stairwell (Treppenabgang), with the Werk door at its back
    b = Builder('CE_D', MATS)
    x0, x1, y0, y1, h = 260, 820, -160, 160, 110
    P.mound(b, (x0 + x1) / 2, 0, 420, 300, h + 10, 'MI_OW_Sod', seed=61, plateau=0.55)
    walls = [((x0, y0 - 30), (x1, y0)), ((x0, y1), (x1, y1 + 30)), ((x0 - 30, y0 - 30), (x0, y1 + 30))]
    for k, (lo, hi) in enumerate(walls):
        top = h + 25 if not d or k == 2 else 40 + 50 * k
        b.box((lo[0], lo[1], -20), (hi[0], hi[1], top), 'MI_OW_Concrete')
    if not d:
        n = 8
        for i in range(n):
            xa = x1 - (x1 - x0 - 60) * i / n
            z = h - (h - 10) * (i + 1) / n
            b.box((xa - (x1 - x0 - 60) / n, y0, z - 8), (xa, y1, z), 'MI_OW_ConcreteDark')
        P.void_quad(b, [(x0 + 2, y0, 0), (x0 + 2, y1, 0), (x0 + 2, y1, 210), (x0 + 2, y0, 210)])
        b.obox((x1 + 70, 0, 140), (14, 340, 220), 0, 'MI_OW_DarkSteel', pitch_deg=-62)
        b.tube([(x0 + 120, y1 + 120, 0), (x0 + 120, y1 + 120, 220)], [14, 14], 8, 'MI_OW_DarkSteel')
        b.tube([(x0 + 120, y1 + 120, 220), (x0 + 120, y1 + 120, 240)], [30, 30], 8, 'MI_OW_DarkSteel')
        P.net_on_poles(b, 62, x0 - 60, -330, x1 + 60, 330, 220, sag=25)
    else:
        hf = P.rubble_heap(b, 63, (x0 + x1) / 2, 0, 330, 210, 140, n=22)
        b.obox((x1 + 160, 140, 8), (14, 340, 220), 50, 'MI_OW_DarkSteel', pitch_deg=88)
        P.net_on_poles(b, 64, x0 - 60, -330, x1 + 60, 330, 220, destroyed=True)
        P.burn_scar(b, 65, (x0 + x1) / 2, 0, 520, 380)
    return b


def ce_E(d):      # vehicle ramp + armoured double door into an earth-covered block
    b = Builder('CE_E', MATS)
    if not d:
        P.mound(b, 350, 0, 520, 560, 420, 'MI_OW_Sod', seed=71, plateau=0.45)
        b.box((560, -260, 0), (620, 260, 330), 'MI_OW_Concrete')
        P.door_plate(b, 624, -100, 0, 195, 280, 0)
        P.door_plate(b, 624, 100, 0, 195, 280, 0)
        b.box((620, -320, 280), (700, 320, 340), 'MI_OW_Concrete')
    else:
        hf = P.mound(b, 350, 0, 520, 560, 380, 'MI_OW_Sod', seed=71, plateau=0.45, crater=230)
        b.box((560, -260, 0), (620, -110, 250), 'MI_OW_Concrete')
        b.box((560, 140, 0), (620, 260, 190), 'MI_OW_Concrete')
        P.void_quad(b, [(570, -110, 0), (570, 140, 0), (570, 140, 230), (570, -110, 230)])
        P.door_plate(b, 900, -60, 0, 195, 280, 20, pitch=84)
        P.rubble_heap(b, 72, 720, 0, 260, 330, 150, n=20)
        P.burn_scar(b, 73, 900, 0, 450, 360)
    for s in (1, -1):
        for i in range(6):
            x = 700 + 120 * i
            top = 330 - 50 * i if not (d and s > 0 and i < 3) else 120
            b.box((x, s * 300 - 20, -20), (x + 120, s * 300 + 20, top), 'MI_OW_Concrete')
        P.mound(b, 1000, s * 520, 380, 200, 220, 'MI_OW_Earth', seed=74 + s, yaw=0)
    P.net_on_poles(b, 76, 640, -360, 1360, 360, 330, destroyed=d, sag=40)
    return b


def ce_F(d):      # escape hatch + vents + periscope cluster
    b = Builder('CE_F', MATS)
    cx = 420
    if not d:
        b.box((cx - 130, -130, -20), (cx + 130, 130, 40), 'MI_OW_Concrete')
        b.box((cx - 70, -70, 40), (cx + 70, 70, 52), 'MI_OW_DarkSteel')
        for (x, y) in ((cx + 260, -160), (cx + 240, 180)):
            b.tube([(x, y, -10), (x, y, 140)], [16, 16], 10, 'MI_OW_DarkSteel')
            b.tube([(x, y, 140), (x, y, 165)], [38, 10], 10, 'MI_OW_DarkSteel')
        b.tube([(cx - 200, 120, 0), (cx - 200, 120, 210)], [7, 7], 6, 'MI_OW_DarkSteel')
        b.box((cx - 212, 108, 200), (cx - 188, 140, 225), 'MI_OW_DarkSteel')
        P.door_plate(b, 8, 0, 0, 150, 220, 0)
        P.net_on_poles(b, 81, cx - 300, -300, cx + 330, 300, 150, sag=20)
    else:
        P.mound(b, cx, 0, 380, 380, 60, 'MI_OW_Earth', seed=82, crater=150)
        P.burn_scar(b, 83, cx, 0, 260, 260, z=-15)
        for (x, y, a) in ((cx + 260, -160, 40), (cx + 240, 180, -55)):
            b.tube([(x, y, -10), (x + 30, y, 60), (x + 30 + 120 * math.cos(math.radians(a)),
                                                    y + 120 * math.sin(math.radians(a)), 70)], [16, 16, 14], 8,
                   'MI_OW_Burnt')
        b.obox((cx + 120, -260, 10), (140, 140, 12), 30, 'MI_OW_DarkSteel', pitch_deg=8)
        P.chunks(b, rng(84), 14, cx, 0, 300, 300, lambda x, y: 0)
        P.door_plate(b, 8, 0, 0, 150, 220, 0)
    return b


CE = dict(CE_A=ce_A, CE_B=ce_B, CE_C=ce_C, CE_D=ce_D, CE_E=ce_E, CE_F=ce_F)


# ================================================================================================ jammer transmitter
def _dugout(b, seed, cx, cy, d, w=380.0, depth=280.0, h=150.0):
    """Sod-covered transmitter cellar with a concrete face and steel door toward the mast (+Y in Blender)."""
    if not d:
        P.mound(b, cx, cy, w * 0.75, depth * 0.8, h, 'MI_OW_Sod', seed=seed, plateau=0.4)
        b.box((cx - w / 2 + 40, cy + depth * 0.45, -20), (cx + w / 2 - 40, cy + depth * 0.45 + 30, h * 0.9),
              'MI_OW_Concrete')
        b.box((cx - 55, cy + depth * 0.45 + 30, 0), (cx + 55, cy + depth * 0.45 + 36, 175), 'MI_OW_DarkSteel')
        b.tube([(cx + w / 3, cy, 0), (cx + w / 3, cy, h + 90)], [9, 9], 8, 'MI_OW_DarkSteel')
    else:
        P.mound(b, cx, cy, w * 0.75, depth * 0.8, h * 0.6, 'MI_OW_Earth', seed=seed, crater=h * 0.9)
        P.chunks(b, rng(seed + 1), 12, cx, cy, w * 0.5, depth * 0.5, lambda x, y: 20)
        P.burn_scar(b, seed + 2, cx, cy, w * 0.8, depth * 0.9)


def _cable(b, a, c, mat='MI_OW_DarkSteel'):
    b.tube([a, c], [2.5, 2.5], 4, mat, smooth=False, cap_start=False, cap_end=False)


def jt_A(d):      # cage antenna on the mast + cellar
    b = Builder('JT_A', MATS)
    if not d:
        z0, z1, R = 480, 900, 85
        for k in range(10):
            a = 2 * math.pi * k / 10
            b.tube([(R * math.cos(a), R * math.sin(a), z0), (R * math.cos(a), R * math.sin(a), z1)], [2.2, 2.2], 4,
                   'MI_OW_RustSteel', smooth=False)
        for z in (z0, (z0 + z1) / 2, z1):
            pts = [(R * math.cos(2 * math.pi * k / 16), R * math.sin(2 * math.pi * k / 16), z) for k in range(17)]
            b.tube(pts, [3.0] * 17, 4, 'MI_OW_DarkSteel', smooth=False, cap_start=False, cap_end=False)
            for a in (0, 120, 240):
                _cable(b, (0, 0, z), (R * math.cos(math.radians(a)), R * math.sin(math.radians(a)), z))
        _cable(b, (0, -30, 0), (0, -30, z0))
    else:
        for k in range(10):
            a = 2 * math.pi * k / 10
            b.tube([(85 * math.cos(a), 85 * math.sin(a), 480), (60 * math.cos(a) + 150, 85 * math.sin(a) - 120, 140),
                    (60 * math.cos(a) + 260, 85 * math.sin(a) - 200, 8)], [2.2] * 3, 4, 'MI_OW_Burnt', smooth=False)
        pts = [(85 * math.cos(2 * math.pi * k / 16) + 230, 85 * math.sin(2 * math.pi * k / 16) - 170, 12)
               for k in range(17)]
        b.tube(pts, [3.0] * 17, 4, 'MI_OW_Burnt', smooth=False, cap_start=False, cap_end=False)
    _dugout(b, 91, 0, -560, d)
    _cable(b, (0, -420, 5), (0, -30, 5))
    return b


def jt_B(d):      # dipole curtain between two extra poles
    b = Builder('JT_B', MATS)
    ya, xa = -760, 560
    if not d:
        ta = P.pole(b, -xa, ya, 0, 980, r=14)
        tb = P.pole(b, xa, ya, 0, 980, r=14)
        for z in (420, 560, 700, 840):
            _cable(b, (-xa, ya, z), (xa, ya, z))
            for k in range(4):
                x = -xa + (2 * xa) * (k + 0.5) / 4
                b.tube([(x - 110, ya, z - 12), (x + 110, ya, z - 12)], [3.5, 3.5], 5, 'MI_OW_RustSteel')
                b.obox((x, ya, z - 6), (14, 14, 14), 0, 'MI_OW_Concrete')
        _cable(b, (0, ya, 420), (0, -520, 160))
    else:
        P.pole(b, -xa, ya, 0, 520, r=14, mat='MI_OW_Burnt')
        b.tube([(-xa, ya, 520), (-xa + 380, ya - 260, 30)], [13, 11], 6, 'MI_OW_Burnt')
        P.pole(b, xa, ya, 0, 980, r=14)
        for z in (420, 560, 700, 840):
            b.tube([(xa, ya, z), (100, ya + 40, 30), (-xa + 200, ya - 120, 10)], [2.5, 2.5, 2.5], 4,
                   'MI_OW_DarkSteel', smooth=False)
    _dugout(b, 92, 0, -420, d, w=320, depth=230, h=130)
    return b


def jt_C(d):      # Yagi stack on the mast top
    b = Builder('JT_C', MATS)
    z0 = 1200
    if not d:
        b.box((-40, -40, z0), (40, 40, z0 + 50), 'MI_OW_DarkSteel')
        b.tube([(0, 0, z0 + 50), (0, 0, z0 + 400)], [6, 5], 8, 'MI_OW_DarkSteel')
        for z in (z0 + 130, z0 + 250, z0 + 370):
            b.tube([(-220, 0, z), (220, 0, z)], [3.5, 3.5], 6, 'MI_OW_DarkSteel')
            for k in range(8):
                x = -200 + 400 * k / 7
                w = 90 if k else 110
                b.tube([(x, -w, z), (x, w, z)], [2, 2], 4, 'MI_OW_RustSteel', smooth=False)
        _cable(b, (0, -35, 10), (0, -35, z0))
    else:
        m = Vector((0, 0, z0 - 300))
        for k, z in enumerate((130, 250, 370)):
            a = Vector((-180 + 60 * k, -150, z0 - 500 + k * 80))
            c = a + Vector((300, -60, -260))
            b.tube([a, c], [3.5, 3.5], 6, 'MI_OW_Burnt')
            for i in range(6):
                p = a + (c - a) * (i / 5)
                b.tube([p + Vector((0, -70, 20)), p + Vector((15, 70, -30))], [2, 2], 4, 'MI_OW_RustSteel',
                       smooth=False)
        b.tube([(0, 0, z0), m + Vector((60, -140, 0))], [6, 5], 8, 'MI_OW_Burnt')
    _dugout(b, 93, 0, -560, d)
    _cable(b, (0, -420, 5), (0, -35, 5))
    return b


def jt_D(d):      # bermed transmitter vault + whips
    b = Builder('JT_D', MATS)
    cx, cy, w, dp, h = 0, -620, 520, 340, 210
    P.mound(b, cx, cy, w * 0.9, dp * 0.95, h - 40, 'MI_OW_Sod', seed=94, crater=0 if not d else 120)
    if not d:
        b.box((cx - w / 2, cy - dp / 2, -20), (cx + w / 2, cy + dp / 2, h), 'MI_OW_Concrete')
        b.box((cx - 60, cy + dp / 2, 0), (cx + 60, cy + dp / 2 + 6, 190), 'MI_OW_DarkSteel')
        for sx in (-1, 1):
            for sy in (-1, 1):
                x, y = cx + sx * (w / 2 - 40), cy + sy * (dp / 2 - 40)
                b.tube([(x, y, h), (x, y, h + 30)], [14, 10], 8, 'MI_OW_Concrete')
                b.tube([(x, y, h + 30), (x, y, h + 680)], [4, 1.5], 5, 'MI_OW_DarkSteel')
        P.net_on_poles(b, 95, cx - w / 2 - 80, cy - dp / 2 - 80, cx + w / 2 + 80, cy + dp / 2 + 80, h + 90, sag=25)
    else:
        b.box((cx - w / 2, cy - dp / 2, -20), (cx - 40, cy + dp / 2, h * 0.7), 'MI_OW_Concrete')
        b.obox((cx + 120, cy, h * 0.55), (300, dp + 40, 40), 0, 'MI_OW_Concrete', roll_deg=12, pitch_deg=-10)
        r = rng(96)
        for sx in (-1, 1):
            for sy in (-1, 1):
                x, y = cx + sx * (w / 2 - 40), cy + sy * (dp / 2 - 40)
                a = Vector((x, y, h))
                dv = Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(0.1, 0.6))).normalized()
                b.tube([a, a + Vector((0, 0, 120)), a + Vector((0, 0, 120)) + dv * 450], [4, 3, 1.5], 5,
                       'MI_OW_Burnt')
        P.burn_scar(b, 97, cx, cy, w * 0.9, dp)
        P.chunks(b, rng(98), 14, cx, cy, w * 0.6, dp * 0.6, lambda x, y: 30)
    return b


def jt_E(d):      # jammer trailer van in a revetment
    b = Builder('JT_E', MATS)
    cx, cy = 0, -640
    for (ox, oy, rx, ry) in ((0, -230, 420, 110), (-330, 0, 110, 300), (330, 0, 110, 300)):
        P.mound(b, cx + ox, cy + oy, rx, ry, 170, 'MI_OW_Earth', seed=101 + ox)
    mat = 'MI_OW_DarkSteel' if not d else 'MI_OW_Burnt'
    tilt = 0 if not d else 7
    if not d:
        b.box((cx - 115, cy - 240, 80), (cx + 115, cy + 240, 320), 'MI_OW_DarkSteel')
        b.box((cx - 118, cy - 200, 150), (cx - 115, cy - 110, 230), 'MI_OW_RustSteel')
        for y in (-140, 140):
            b.tube([(cx - 125, cy + y, 45), (cx + 125, cy + y, 45)], [45, 45], 12, 'MI_OW_Burnt')
        b.tube([(cx, cy + 160, 320), (cx, cy + 160, 900)], [5, 3], 6, 'MI_OW_DarkSteel')
        b.tube([(cx - 260, cy + 160, 820), (cx + 260, cy + 160, 820)], [3, 3], 5, 'MI_OW_RustSteel')
        b.tube([(cx - 200, cy + 160, 680), (cx + 200, cy + 160, 680)], [3, 3], 5, 'MI_OW_RustSteel')
        b.box((cx + 150, cy + 280, 0), (cx + 260, cy + 400, 110), 'MI_OW_ConcreteDark')
        P.net_on_poles(b, 104, cx - 330, cy - 330, cx + 330, cy + 330, 380, sag=30)
    else:
        b.obox((cx, cy, 150), (230, 480, 180), 0, mat, roll_deg=tilt, pitch_deg=3)
        b.obox((cx + 40, cy + 60, 250), (190, 300, 20), 15, mat, roll_deg=-12)
        b.tube([(cx - 125, cy + 140, 40), (cx + 125, cy + 140, 40)], [45, 45], 12, 'MI_OW_Burnt')
        b.tube([(cx, cy + 160, 240), (cx + 400, cy + 380, 30)], [5, 3], 6, 'MI_OW_Burnt')
        P.burn_scar(b, 105, cx, cy, 460, 480)
        P.net_on_poles(b, 106, cx - 330, cy - 330, cx + 330, cy + 330, 380, destroyed=True)
    return b


def jt_F(d):      # umbrella wires to short posts + tuning hut
    b = Builder('JT_F', MATS)
    top = (0, 0, 1180)
    for k, a in enumerate((30, 150, 270)):
        for da in (-14, 0, 14):
            aa = math.radians(a + da)
            x, y = 560 * math.cos(aa), 560 * math.sin(aa)
            if not d:
                P.pole(b, x, y, 0, 220, r=7)
                _cable(b, top, (x, y, 220))
            elif da == 0:
                b.tube([(x, y, 0), (x + 120 * math.cos(aa), y + 120 * math.sin(aa), 25)], [7, 6], 6, 'MI_OW_Burnt')
            else:
                P.pole(b, x, y, 0, 220, r=7)
                b.tube([(x, y, 220), (x * 0.6, y * 0.6, 40), (x * 0.2, y * 0.2, 200)], [2.5] * 3, 4,
                       'MI_OW_DarkSteel', smooth=False)
    _dugout(b, 107, 0, -330, d, w=300, depth=220, h=120)
    return b


JT = dict(JT_A=jt_A, JT_B=jt_B, JT_C=jt_C, JT_D=jt_D, JT_E=jt_E, JT_F=jt_F)


# ================================================================================================ rail entrance
def re_A(d):      # sliding armoured gate inside the mouth
    b = Builder('RE_A', MATS)
    x = BACK_X + 60
    if not d:
        b.box((x, -OPEN_W + 10, 0), (x + 30, OPEN_W - 10, 470), 'MI_OW_DarkSteel')
        for y in range(-400, 401, 100):
            b.box((x + 30, y - 6, 10), (x + 38, y + 6, 460), 'MI_OW_RustSteel')
        b.box((x - 10, -OPEN_W - 300, 470), (x + 40, OPEN_W, 500), 'MI_OW_DarkSteel')
        b.tube([(x + 45, 380, 420), (x + 60, 380, 420)], [18, 18], 8, 'MI_OW_RustSteel')
    else:
        b.obox((x + 140, -200, 170), (30, 480, 470), 0, 'MI_OW_Burnt', pitch_deg=-28, roll_deg=6)
        b.obox((x + 60, 250, 200), (30, 400, 420), 0, 'MI_OW_DarkSteel', pitch_deg=12, roll_deg=-10)
        P.rubble_heap(b, 111, x + 200, 0, 300, 450, 160, n=14)
    b.box((60, -140, 3), (300, 140, 20), 'MI_OW_ConcreteDark')
    return b


def re_B(d):      # camouflage curtain hung from the lintel
    b = Builder('RE_B', MATS)
    r = rng(121)
    n = 12
    for k in range(n):
        y0 = -OPEN_W + 2 * OPEN_W * k / n
        y1 = y0 + 2 * OPEN_W / n + 8
        x = HW_X1 - 25 + 8 * math.sin(k * 1.7)
        if not d:
            b.face([(x, y0, 0), (x + 6 * r.uniform(-1, 1), y1, 0), (x, y1, OPEN_H), (x, y0, OPEN_H)], 'MI_OW_CamoNet')
            b.face([(x, y0, OPEN_H), (x, y1, OPEN_H), (x, y1, 0), (x, y0, 0)], 'MI_OW_CamoNet')
        else:
            z0 = OPEN_H - r.uniform(60, 260)
            b.face([(x, y0, z0), (x, y1, z0 + r.uniform(-40, 40)), (x, y1, OPEN_H), (x, y0, OPEN_H)], 'MI_OW_Burnt')
            b.face([(x, y0, OPEN_H), (x, y1, OPEN_H), (x, y1, z0), (x, y0, z0)], 'MI_OW_Burnt')
    b.box((HW_X1 - 40, -OPEN_W, OPEN_H - 30), (HW_X1 - 20, OPEN_W, OPEN_H), 'MI_OW_DarkSteel')
    if not d:
        b.box((520, 330, 0), (900, 560, 100), 'MI_OW_Concrete')
        b.box((560, 360, 100), (700, 520, 200), 'MI_OW_DarkSteel')
    else:
        P.rubble_heap(b, 122, 700, 430, 300, 180, 90, n=14)
        P.burn_scar(b, 123, 500, 0, 420, 460)
    return b


def re_C(d):      # loading platform + armoured rail car under a net
    b = Builder('RE_C', MATS)
    if not d:
        b.box((340, 200, 0), (930, 470, 110), 'MI_OW_Concrete')
        b.box((380, -120, 60), (980, 120, 230), 'MI_OW_DarkSteel')
        b.obox((680, 0, 270), (420, 200, 80), 0, 'MI_OW_DarkSteel')
        b.tube([(680, 0, 330), (680, 0, 380)], [60, 50], 10, 'MI_OW_DarkSteel')
        b.tube([(720, 0, 360), (960, 0, 360)], [6, 6], 8, 'MI_OW_DarkSteel')
        for x in (440, 600, 760, 920):
            b.tube([(x, -90, 40), (x, 90, 40)], [38, 38], 10, 'MI_OW_RustSteel')
        P.net_on_poles(b, 131, 330, -330, 1020, 480, 420, sag=40)
    else:
        b.box((340, 200, 0), (700, 470, 110), 'MI_OW_Concrete')
        P.rubble_heap(b, 132, 820, 330, 200, 160, 110, n=10)
        b.obox((700, -40, 120), (600, 240, 170), 14, 'MI_OW_Burnt', roll_deg=-16)
        b.obox((640, -150, 50), (380, 190, 70), 40, 'MI_OW_Burnt', roll_deg=60)
        P.burn_scar(b, 133, 680, 0, 520, 420)
        P.net_on_poles(b, 134, 330, -330, 1020, 480, 420, destroyed=True)
    return b


def re_D(d):      # signal box dug into the berm + signal gantry
    b = Builder('RE_D', MATS)
    hx, hy = 520, 1050
    if not d:
        P.mound(b, hx - 150, hy + 80, 380, 300, 330, 'MI_OW_Sod', seed=141, plateau=0.5)
        b.box((hx, hy - 160, -20), (hx + 40, hy + 160, 270), 'MI_OW_Concrete')
        for y in (-90, 20):
            P.void_quad(b, [(hx + 41, hy + y, 150), (hx + 41, hy + y + 70, 150), (hx + 41, hy + y + 70, 210),
                            (hx + 41, hy + y, 210)])
        P.sandbags(b, 142, (hx + 140, hy - 200), (hx + 140, hy + 180), 110)
        for y in (-320, 320):
            b.box((1080, y - 20, 0), (1120, y + 20, 560), 'MI_OW_DarkSteel')
        b.box((1070, -340, 540), (1130, 340, 580), 'MI_OW_DarkSteel')
        for y in (-150, 150):
            b.box((1085, y - 30, 440), (1115, y + 30, 540), 'MI_OW_DarkSteel')
    else:
        P.mound(b, hx - 150, hy + 80, 380, 300, 250, 'MI_OW_Earth', seed=141, crater=200)
        b.box((hx, hy - 160, -20), (hx + 40, hy + 20, 160), 'MI_OW_Concrete')
        P.rubble_heap(b, 143, hx + 120, hy, 220, 260, 140, n=12)
        b.box((1080, -340, 0), (1120, -300, 560), 'MI_OW_DarkSteel')
        b.obox((900, 250, 30), (60, 760, 50), 35, 'MI_OW_Burnt', roll_deg=8)
        P.sandbags(b, 144, (hx + 140, hy - 200), (hx + 140, hy + 180), 110, scatter=0.6)
        P.burn_scar(b, 145, hx + 80, hy, 380, 360)
    return b


def re_E(d):      # secondary narrow-gauge adit beside the portal
    b = Builder('RE_E', MATS)
    ax, ay = 420, 1500
    P.mound(b, ax - 300, ay, 520, 420, 420 if not d else 300, 'MI_OW_Earth', seed=151, plateau=0.3,
            crater=0 if not d else 160)
    if not d:
        for y in (-170, 170):
            b.box((ax - 20, ay + y - 22, 0), (ax + 20, ay + y + 22, 300), 'MI_OW_Wood')
        b.box((ax - 30, ay - 210, 300), (ax + 30, ay + 210, 350), 'MI_OW_Wood')
        P.void_quad(b, [(ax - 25, ay - 148, 0), (ax - 25, ay + 148, 0), (ax - 25, ay + 148, 300),
                        (ax - 25, ay - 148, 300)])
        for y in (-40, 40):
            b.box((ax - 30, ay + y - 3, 0), (ax + 700, ay + y + 3, 12), 'MI_OW_RustSteel')
        b.box((ax + 220, ay - 70, 20), (ax + 380, ay + 70, 120), 'MI_OW_RustSteel')
        P.net_on_poles(b, 152, ax - 20, ay - 300, ax + 560, ay + 300, 260, sag=30)
    else:
        b.obox((ax + 40, ay - 120, 120), (40, 44, 320), 0, 'MI_OW_Wood', pitch_deg=35)
        b.obox((ax + 80, ay + 60, 60), (60, 420, 50), 20, 'MI_OW_Wood', roll_deg=15)
        P.rubble_heap(b, 153, ax + 120, ay, 300, 300, 230, n=14, earth='MI_OW_Earth')
        b.obox((ax + 380, ay - 160, 40), (160, 140, 90), 60, 'MI_OW_Burnt', roll_deg=70)
        P.net_on_poles(b, 154, ax - 20, ay - 300, ax + 560, ay + 300, 260, destroyed=True)
    return b


RE = dict(RE_A=re_A, RE_B=re_B, RE_C=re_C, RE_D=re_D, RE_E=re_E)
