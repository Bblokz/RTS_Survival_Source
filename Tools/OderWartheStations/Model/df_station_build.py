"""Oder-Warthe direction-finder station, design F "Peilstelle" (loop hut inside a small Adcock square).

    Blender5.0.exe -b --factory-startup -P df_station_build.py -- [--preview]

Builds with the Oder-Warthe kit helpers (Source/Tools/OderWartheMap/assets/ow_blender_lib.py): centimetres, +X = front
(the open yard, flag and holder sign face +X), +Z up, pivot = capture centre on the ground. Writes to
D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Stations/ (FBX/, OW_DFStation_Peilstelle.blend,
df_station_report.json, previews/).

Meshes (one Blueprint component each, see ue_build_df_station_bp.py):
  SM_OW_DFStation_Base        hut, four Adcock masts + guys + feeders, generator lean-to, flag plinth + pole, sign
                              posts, lamp bracket. UCX: hut, generator, mast feet, flag plinth (guys and feeders do not
                              collide, so units walk through the square).
  SM_OW_DFStation_LoopAerial  the rotating loop (shaft, ring, spoke); pivot = shaft foot on the roof rotator.
  SM_OW_DFStation_SignEnemy   board with the red star (tag EnemyHeld); pivot = board bottom centre.
  SM_OW_DFStation_SignPlayer  white board with the black Balkenkreuz (tag PlayerHeld).
  SM_OW_DFStation_LampGlow    the lit lamp lens over the hut door (tag PlayerHeld).
The flag itself is the supply point's SM_SupplyPoint_Flag (banner materials swap per holder in C++).
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/OderWartheMap/assets')
import ow_blender_lib as L  # noqa: E402
from ow_blender_lib import Builder, box_pts, obox_pts, v3  # noqa: E402

OUT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Stations/'
FBX = OUT + 'FBX/'
PREVIEW = '--preview' in sys.argv

# Station-only paint instances (created in Unreal by ue_import_df_station.py as M_OW_Kit instances).
L.MATS.update({
    'MI_OW_DF_RedPaint': dict(tex='T_OW_RustSteel_D', tint=(0.78, 0.10, 0.08), rough=0.72, metal=0.15, tile=150.0),
    'MI_OW_DF_WhitePaint': dict(tex='T_OW_Wood_D', tint=(1.0, 1.0, 0.97), rough=0.85, metal=0.0, tile=200.0),
    'MI_OW_DF_BlackPaint': dict(tex='T_OW_Concrete_D', tint=(0.07, 0.07, 0.07), rough=0.8, metal=0.0, tile=300.0),
})

# ---- layout (cm). Concept F (metres, x right / y back) -> here X = -100 y, Y = -100 x.
MASTS = [(350.0, -600.0), (350.0, 600.0), (-850.0, 600.0), (-850.0, -600.0)]
MAST_H = 700.0
HUT = dict(cx=-560.0, cy=-50.0, w=340.0, d=420.0, h=250.0, ridge=95.0)   # w along X, d along Y; door on +X
ROTATOR_TOP = None          # set by hut(): top of the roof rotator = loop pivot z
GEN = (-380.0, -980.0)
FLAG_POLE = (170.0, 300.0)
FLAG_PLINTH_H = 45.0
FLAG_POLE_H = 750.0         # above the plinth
SIGN = (270.0, -300.0)
SIGN_YAW = 15.0             # board normal turned this much from +X toward +Y
SIGN_BOARD_Z = 95.0         # board bottom
LAMP = None                 # lamp lens centre, set by hut()
LAYOUT = {}


def cyl(b, c, r, z0, z1, mat, sides=12, smooth=True, cap=True):
    return b.tube([(c[0], c[1], z0), (c[0], c[1], z1)], [r, r], sides, mat, smooth=smooth, cap_start=cap, cap_end=cap)


def torus(b, center, R, r, mat, yaw=0.0, segs=28, sides=6):
    """Vertical ring (axis horizontal) yawed about Z."""
    m = Matrix.Translation(v3(center)) @ Matrix.Rotation(math.radians(yaw), 4, 'Z') @ Matrix.Rotation(
        math.radians(90), 4, 'X')
    pts = [m @ Vector((R * math.cos(2 * math.pi * k / segs), R * math.sin(2 * math.pi * k / segs), 0))
           for k in range(segs + 1)]
    b.tube(pts, [r] * len(pts), sides, mat, smooth=True, cap_start=False, cap_end=False)


def guy(b, top, anchor, r=1.4):
    b.tube([top, anchor], [r, r], 3, 'MI_OW_DarkSteel', smooth=False, cap_start=False, cap_end=False)


def anchor_block(b, p, s=40):
    b.box((p[0] - s / 2, p[1] - s / 2, -15), (p[0] + s / 2, p[1] + s / 2, 12), 'MI_OW_ConcreteDark')


def rot2(o, d, yaw):
    a = math.radians(yaw)
    return (o[0] + d[0] * math.cos(a) - d[1] * math.sin(a), o[1] + d[0] * math.sin(a) + d[1] * math.cos(a))


# ------------------------------------------------------------------------------------------------- base parts
def hut(b):
    global ROTATOR_TOP, LAMP
    cx, cy, w, d, h, ridge = (HUT[k] for k in ('cx', 'cy', 'w', 'd', 'h', 'ridge'))
    b.box((cx - w / 2 - 14, cy - d / 2 - 14, -25), (cx + w / 2 + 14, cy + d / 2 + 14, 28), 'MI_OW_ConcreteDark')
    b.box((cx - w / 2, cy - d / 2, 28), (cx + w / 2, cy + d / 2, h), 'MI_OW_Wood', skip=('+z',))
    # plank battens on the long walls (vertical boards read at RTS distance)
    for side in (-1, 1):
        y = cy + side * (d / 2 + 2)
        for k in range(1, 7):
            x = cx - w / 2 + k * w / 7
            b.box((x - 4, min(y, y - side * 2), 30), (x + 4, max(y, y - side * 2), h - 6), 'MI_OW_Wood')
    for side in (-1, 1):
        x = cx + side * (w / 2 + 2)
        for k in range(1, 9):
            yy = cy - d / 2 + k * d / 9
            b.box((min(x, x - side * 2), yy - 4, 30), (max(x, x - side * 2), yy + 4, h - 6), 'MI_OW_Wood')
    # gable roof, ridge along X, rusted iron sheets
    ov = 24.0
    x0, x1 = cx - w / 2 - ov, cx + w / 2 + ov
    y0, y1 = cy - d / 2 - ov, cy + d / 2 + ov
    zt, eave = h + ridge, h - 14
    b.face([(x0, y0, eave), (x1, y0, eave), (x1, cy, zt), (x0, cy, zt)], 'MI_OW_RustSteel')
    b.face([(x1, y1, eave), (x0, y1, eave), (x0, cy, zt), (x1, cy, zt)], 'MI_OW_RustSteel')
    b.face([(x0, cy, zt), (x1, cy, zt), (x1, y0, eave), (x0, y0, eave)], 'MI_OW_RustSteel')   # undersides
    b.face([(x1, cy, zt), (x0, cy, zt), (x0, y1, eave), (x1, y1, eave)], 'MI_OW_RustSteel')
    b.face([(cx + w / 2, cy - d / 2, h), (cx + w / 2, cy + d / 2, h), (cx + w / 2, cy, zt - 6)], 'MI_OW_Wood')
    b.face([(cx - w / 2, cy + d / 2, h), (cx - w / 2, cy - d / 2, h), (cx - w / 2, cy, zt - 6)], 'MI_OW_Wood')
    b.box((x0, cy - 9, zt - 4), (x1, cy + 9, zt + 6), 'MI_OW_DarkSteel')                       # ridge cap
    # door and window on the front gable (+X), window on the -Y side
    fx = cx + w / 2
    b.box((fx, cy + 30, 30), (fx + 5, cy + 125, 225), 'MI_OW_DarkSteel')
    b.box((fx + 5, cy + 112, 115), (fx + 9, cy + 120, 135), 'MI_OW_RustSteel')                 # handle
    b.box((fx, cy - 135, 125), (fx + 4, cy - 35, 195), 'MI_OW_Wood')
    b.box((fx + 4, cy - 127, 132), (fx + 6, cy - 43, 188), 'MI_OW_Void')
    b.box((cx - 60, cy - d / 2 - 6, 125), (cx + 60, cy - d / 2, 190), 'MI_OW_Wood')
    b.box((cx - 52, cy - d / 2 - 8, 132), (cx + 52, cy - d / 2 - 6, 183), 'MI_OW_Void')
    # lamp over the door: bracket + housing (the lit lens is SM_OW_DFStation_LampGlow)
    lz = 238.0
    b.box((fx, cy + 70, lz - 4), (fx + 34, cy + 80, lz + 4), 'MI_OW_DarkSteel')
    b.tube([(fx + 30, cy + 75, lz), (fx + 30, cy + 75, lz - 22)], [13, 9], 10, 'MI_OW_DarkSteel', smooth=True)
    LAMP = (fx + 30, cy + 75, lz - 26)
    # stovepipe on the back slope
    cyl(b, (cx - 100, cy + 90), 8, h, zt + 80, 'MI_OW_DarkSteel', 8)
    b.box((cx - 115, cy + 75, zt + 86), (cx - 85, cy + 105, zt + 90), 'MI_OW_DarkSteel')
    # roof rotator on the ridge: housing, hand-wheel crank box, turntable
    rz = zt + 4
    b.box((cx - 40, cy - 40, rz - 20), (cx + 40, cy + 40, rz + 40), 'MI_OW_DarkSteel')
    cyl(b, (cx, cy), 30, rz + 40, rz + 52, 'MI_OW_RustSteel', 14)
    ROTATOR_TOP = rz + 52
    LAYOUT['hut'] = dict(HUT, roof_top=zt, loop_pivot=[cx, cy, ROTATOR_TOP], lamp=list(LAMP))
    return zt


def mast(b, x, y, idx):
    """Banded steel tube mast on a concrete foot: crossarm with insulators, whip, three guys outward."""
    b.box((x - 32, y - 32, -15), (x + 32, y + 32, 22), 'MI_OW_Concrete')
    b.tube([(x, y, 22), (x, y, MAST_H)], [12.0, 7.0], 10, 'MI_OW_DarkSteel', smooth=True)
    for z0 in (150.0, 350.0, 550.0):
        rr = 12.0 - 5.0 * (z0 / MAST_H)
        b.tube([(x, y, z0), (x, y, z0 + 60)], [rr + 0.8, rr + 0.7], 10, 'MI_OW_DF_RedPaint', smooth=True,
               cap_start=True, cap_end=True)
    # crossarm square to the station centre line, insulators, whip
    out = math.degrees(math.atan2(y - (-250.0 * 0), x - (-250.0)))
    arm = out + 90.0
    b.obox((x, y, MAST_H - 45), (12, 160, 12), arm, 'MI_OW_DarkSteel')
    for off in (-68.0, 68.0):
        px, py = rot2((x, y), (0.0, off), arm)
        cyl(b, (px, py), 5, MAST_H - 39, MAST_H - 16, 'MI_OW_ConcreteDark', 6)
    cyl(b, (x, y), 3.5, MAST_H, MAST_H + 130, 'MI_OW_DarkSteel', 6)
    for da in (-60.0, 0.0, 60.0):
        ax, ay = rot2((x, y), (300.0, 0.0), out + da)
        guy(b, (x, y, MAST_H * 0.8), (ax, ay, 8))
        anchor_block(b, (ax, ay))
    # feeder towards the hut on two low posts
    cx, cy = HUT['cx'], HUT['cy']
    tx = cx + (HUT['w'] / 2 + 10) * (1 if x > cx else -1)
    ty = cy + (HUT['d'] / 2 - 30) * (1 if y > cy else -1)
    fa = Vector((x, y, 160.0))
    fb = Vector((tx, ty, 160.0))
    b.tube([fa, fb], [2.0, 2.0], 4, 'MI_OW_DarkSteel', smooth=False)
    for f in (0.33, 0.66):
        p = fa.lerp(fb, f)
        b.tube([(p.x, p.y, -10), (p.x, p.y, p.z + 4)], [6, 5], 6, 'MI_OW_Wood')
        b.obox((p.x, p.y, p.z + 4), (14, 14, 6), 0, 'MI_OW_ConcreteDark')


def generator(b):
    x, y = GEN
    m = Matrix.Translation(Vector((x, y, 0))) @ Matrix.Rotation(math.radians(0.0), 4, 'Z')

    def bx(lo, hi, mat):
        b.box(lo, hi, mat, xform=m)
    bx((-120, -55, -10), (120, 55, 12), 'MI_OW_Wood')                # timber skid
    bx((-100, -40, 12), (15, 40, 95), 'MI_OW_RustSteel')             # engine
    bx((15, -42, 12), (85, 42, 82), 'MI_OW_DarkSteel')               # dynamo housing
    bx((-110, -34, 18), (-100, 34, 90), 'MI_OW_DarkSteel')           # radiator
    p = m @ Vector((-30, 22, 95))
    b.tube([p, p + Vector((0, 0, 70)), p + Vector((14, 0, 84))], [4, 4, 4], 6, 'MI_OW_DarkSteel', smooth=False)
    # lean-to: four posts, sloped tin roof falling to the back (-Y side = away from the hut)
    for px in (-150.0, 150.0):
        for py, top in ((-95.0, 195.0), (125.0, 225.0)):
            cyl(b, (x + px, y + py), 7, -10, top, 'MI_OW_Wood', 6)
    b.face([(x - 175, y - 120, 190), (x + 175, y - 120, 190), (x + 175, y + 150, 232), (x - 175, y + 150, 232)],
           'MI_OW_RustSteel')
    b.face([(x + 175, y - 120, 186), (x - 175, y - 120, 186), (x - 175, y + 150, 228), (x + 175, y + 150, 228)],
           'MI_OW_RustSteel')
    # fuel drums and a jerrycan stack
    for dx, dy in ((-160.0, -150.0), (-95.0, -165.0), (-128.0, -215.0)):
        cyl(b, (x + dx, y + dy), 29, 0, 88, 'MI_OW_RustSteel', 12)
    # power cable on two posts from the generator to the hut's -Y wall
    hx, hy = HUT['cx'] + 60, HUT['cy'] - HUT['d'] / 2 - 4
    a = Vector((x + 40, y + 120, 200.0))
    c = Vector((hx, hy, 215.0))
    mid = a.lerp(c, 0.5)
    b.tube([a, mid - Vector((0, 0, 25)), c], [1.8, 1.8, 1.8], 4, 'MI_OW_DarkSteel', smooth=False)
    b.tube([(mid.x, mid.y, -10), (mid.x, mid.y, mid.z - 22)], [7, 6], 6, 'MI_OW_Wood')


def flag_pole(b):
    x, y = FLAG_POLE
    b.box((x - 50, y - 50, -20), (x + 50, y + 50, FLAG_PLINTH_H), 'MI_OW_Concrete')
    b.box((x - 56, y - 56, -20), (x + 56, y + 56, 8), 'MI_OW_ConcreteDark')
    top = FLAG_PLINTH_H + FLAG_POLE_H
    b.tube([(x, y, FLAG_PLINTH_H), (x, y, top)], [8.0, 5.5], 10, 'MI_OW_DarkSteel', smooth=True)
    cyl(b, (x, y), 11, top, top + 14, 'MI_OW_CastSteel', 10)                 # finial truck
    b.box((x - 3, y - 14, 120), (x + 3, y - 8, 150), 'MI_OW_RustSteel')      # halyard cleat
    LAYOUT['flag'] = dict(pole=[x, y], pole_top=top, plinth_top=FLAG_PLINTH_H)


def sign_posts(b):
    x, y = SIGN
    for off in (-75.0, 75.0):
        px, py = rot2((x, y), (0.0, off), SIGN_YAW)
        b.tube([(px, py, -20), (px, py, 215)], [6.5, 6.0], 6, 'MI_OW_Wood', smooth=False)
    LAYOUT['sign'] = dict(xy=[x, y], yaw=SIGN_YAW, board_z=SIGN_BOARD_Z)


def crates(b):
    for k, (dx, dy, dz) in enumerate(((0, 0, 0), (85, 10, 0), (40, 5, 60))):
        b.obox((-860 + 120 + dx, -180 + dy, dz + 30), (80, 60, 58), 8 * k, 'MI_OW_Wood')


# ------------------------------------------------------------------------------------------------- moving parts
LOOP_R = 105.0
LOOP_SHAFT = 95.0


def build_loop():
    """Pivot at the shaft foot (= rotator top). Ring plane contains local X (yaw 0)."""
    b = Builder('SM_OW_DFStation_LoopAerial', ['MI_OW_DarkSteel', 'MI_OW_RustSteel'])
    cyl(b, (0, 0), 20, 0, 14, 'MI_OW_RustSteel', 12)
    cyl(b, (0, 0), 6.5, 14, LOOP_SHAFT, 'MI_OW_DarkSteel', 8)
    b.obox((0, 0, LOOP_SHAFT + 2), (30, 22, 14), 0, 'MI_OW_RustSteel')       # clamp
    cz = LOOP_SHAFT + LOOP_R
    torus(b, (0, 0, cz), LOOP_R, 6.0, 'MI_OW_DarkSteel', yaw=0.0)
    b.tube([(0, 0, cz - LOOP_R), (0, 0, cz + LOOP_R)], [2.6, 2.6], 4, 'MI_OW_DarkSteel', smooth=False)
    b.tube([(-LOOP_R, 0, cz), (LOOP_R, 0, cz)], [2.2, 2.2], 4, 'MI_OW_DarkSteel', smooth=False)
    cyl(b, (0, 0), 9, cz + LOOP_R - 4, cz + LOOP_R + 10, 'MI_OW_RustSteel', 8)   # top bearing cap
    return b


def build_sign(player):
    """Board 180 x 110 cm, facing +X; pivot at the board bottom centre."""
    name = 'SM_OW_DFStation_SignPlayer' if player else 'SM_OW_DFStation_SignEnemy'
    b = Builder(name, ['MI_OW_DF_WhitePaint' if player else 'MI_OW_Wood'])
    board = 'MI_OW_DF_WhitePaint' if player else 'MI_OW_Wood'
    b.box((-4, -90, 0), (4, 90, 110), board)
    b.box((-6, -92, 106), (6, 92, 114), 'MI_OW_Wood')                     # top rail
    if player:
        b.box((4, -42, 45), (6, 42, 65), 'MI_OW_DF_BlackPaint')            # Balkenkreuz
        b.box((4, -10, 13), (6, 10, 97), 'MI_OW_DF_BlackPaint')
        b.box((4, -46, 41), (5, 46, 69), 'MI_OW_DF_WhitePaint')            # white outline (reads on dark board too)
        b.box((4, -14, 9), (5, 14, 101), 'MI_OW_DF_WhitePaint')
    else:
        pts = []
        for i in range(10):
            a = math.radians(90 + i * 36)
            r = 44.0 if i % 2 == 0 else 18.0
            pts.append((r * math.cos(a), r * math.sin(a)))
        cz = 57.0
        front = [(5.5, -px, cz + pz) for px, pz in pts]
        back = [(4.0, -px, cz + pz) for px, pz in pts]
        b.face(front, 'MI_OW_DF_RedPaint', uvs=[(0.5 + p[1] / 100, 0.5 + p[2] / 100) for p in front])
        for i in range(10):
            j = (i + 1) % 10
            b.face([back[i], back[j], front[j], front[i]], 'MI_OW_DF_RedPaint')
        # stencilled number plate under the star
        b.box((4, -30, 6), (5.5, 30, 14), 'MI_OW_DF_RedPaint')
    return b


def build_lamp_glow():
    b = Builder('SM_OW_DFStation_LampGlow', ['MI_RusTent_WindowGlow'])
    b.tube([(0, 0, 6), (0, 0, -4)], [8.5, 6.0], 12, 'MI_RusTent_WindowGlow', smooth=True)
    return b


# ------------------------------------------------------------------------------------------------- export
def export(b, hulls=(), sharp=40.0):
    obj = b.to_object(sharp)
    info = L.export_fbx(obj, FBX + b.name + '.fbx', hulls)
    print('BUILT', b.name, json.dumps(info))
    return obj, info


def preview(path, objs_xform, target, distance, az=-40.0, el=48.0, lens=50.0, res=(1400, 900)):
    """objs_xform: list of (object, matrix_world). Cycles CPU still from the kit's preview helper."""
    for obj, mw in objs_xform:
        obj.matrix_world = mw
    L.render_preview(path, target=target, distance=distance, azimuth_deg=az, elevation_deg=el, lens=lens,
                     resolution=res)


def main():
    L.reset_scene()
    os.makedirs(FBX, exist_ok=True)
    report = {}

    base = Builder('SM_OW_DFStation_Base', ['MI_OW_Wood', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_Concrete',
                                            'MI_OW_ConcreteDark', 'MI_OW_Void', 'MI_OW_DF_RedPaint',
                                            'MI_OW_CastSteel'])
    zt = hut(base)
    for i, (x, y) in enumerate(MASTS):
        mast(base, x, y, i)
    generator(base)
    flag_pole(base)
    sign_posts(base)
    crates(base)
    c = HUT
    hulls = [box_pts((c['cx'] - c['w'] / 2 - 14, c['cy'] - c['d'] / 2 - 14, -25),
                     (c['cx'] + c['w'] / 2 + 14, c['cy'] + c['d'] / 2 + 14, zt)),
             box_pts((GEN[0] - 125, GEN[1] - 60, -10), (GEN[0] + 125, GEN[1] + 60, 100)),
             box_pts((FLAG_POLE[0] - 56, FLAG_POLE[1] - 56, -20), (FLAG_POLE[0] + 56, FLAG_POLE[1] + 56, 260))]
    for x, y in MASTS:
        hulls.append(box_pts((x - 32, y - 32, -15), (x + 32, y + 32, 240)))
    base_obj, report['SM_OW_DFStation_Base'] = export(base, hulls)
    loop_obj, report['SM_OW_DFStation_LoopAerial'] = export(build_loop())
    se_obj, report['SM_OW_DFStation_SignEnemy'] = export(build_sign(False))
    sp_obj, report['SM_OW_DFStation_SignPlayer'] = export(build_sign(True))
    lamp_obj, report['SM_OW_DFStation_LampGlow'] = export(build_lamp_glow())
    LAYOUT['loop'] = dict(radius=LOOP_R, shaft=LOOP_SHAFT, top=LAYOUT['hut']['loop_pivot'][2] + LOOP_SHAFT + 2 * LOOP_R)
    LAYOUT['masts'] = [list(m) for m in MASTS]
    LAYOUT['mast_height'] = MAST_H
    LAYOUT['generator'] = list(GEN)
    report['layout'] = LAYOUT
    with open(OUT + 'df_station_report.json', 'w') as f:
        json.dump(report, f, indent=1)

    # assemble the station (both holder states) for the .blend and the previews
    pivot = Vector(LAYOUT['hut']['loop_pivot'])
    sx, sy = SIGN
    sign_mw = Matrix.Translation(Vector((sx, sy, SIGN_BOARD_Z))) @ Matrix.Rotation(math.radians(SIGN_YAW), 4, 'Z')
    loop_obj.matrix_world = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(35), 4, 'Z')
    se_obj.matrix_world = sign_mw
    sp_obj.matrix_world = sign_mw
    lamp_obj.matrix_world = Matrix.Translation(Vector(LAMP))
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=OUT + 'OW_DFStation_Peilstelle.blend', compress=True)
    if PREVIEW:
        prev = OUT + 'previews/'
        sp_obj.hide_render = True
        lamp_obj.hide_render = True
        L.render_preview(prev + 'peilstelle_enemy.png', target=(-250, -150, 250), distance=3300, azimuth_deg=-40,
                         elevation_deg=48, resolution=(1400, 900))
        sp_obj.hide_render = False
        lamp_obj.hide_render = False
        se_obj.hide_render = True
        L.render_preview(prev + 'peilstelle_player.png', target=(-250, -150, 250), distance=3300, azimuth_deg=-40,
                         elevation_deg=48, resolution=(1400, 900))
        se_obj.hide_render = False
        sp_obj.hide_render = True
        L.render_preview(prev + 'peilstelle_rear.png', target=(-250, -150, 250), distance=3300, azimuth_deg=140,
                         elevation_deg=40, resolution=(1000, 650))
    print('DF_STATION_DONE', json.dumps(LAYOUT))


main()
