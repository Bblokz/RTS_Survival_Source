"""Supply point concept parts (greybox, runs inside Blender through the Breakthrough concept tools).

Frame: metres, x = right, y = back (away from the RTS camera), z = up, origin = the supply point's origin.
The RTS camera looks from the front-right (+x, -y). A truck heading of 0 deg drives along +x.

The truck is the game's real Opel Blitz (SK_Ger_Opel FBX, 6.42 x 2.44 x 2.53 m). Its bed floor is at 1.18 m and it
has four canvas bows (2.53 m) across the bed, 1.0 m apart, so every load drops between two bows: the three bed slots
are the three gaps between the bows (truck-local x -0.28 / -1.28 / -2.29 from the truck's bottom centre;
bows at +0.24 / -0.80 / -1.75 / -2.82, ~0.07 m thick, crown 2.49-2.53 m, so each gap is about 0.95 m clear).
Resource placeholders keep the concept-kit colours (yellow radixite, light blue metal, rust vehicle parts) but the
shapes follow the HQ fill meshes: crystal clusters on a crushed-crystal bed, plate stacks + pig-iron cribs + I-beams,
road-wheel stacks + folded track + gearboxes.
"""
import math
import os

import bpy
import concept_kit
from mathutils import Matrix, Vector

P = concept_kit.PAL
P.setdefault('truck', (0.36, 0.40, 0.29, 1))
P.setdefault('flag', (0.86, 0.10, 0.10, 1))
P.setdefault('flag_ghost', (0.97, 0.78, 0.78, 1))
P.setdefault('lane', (0.20, 0.26, 0.40, 1))
P.setdefault('apron', (0.82, 0.76, 0.63, 1))
P.setdefault('cable', (0.06, 0.06, 0.06, 1))
P.setdefault('track', (0.16, 0.12, 0.09, 1))
P.setdefault('ui', (0.20, 0.78, 0.35, 1))
P.setdefault('timber', (0.47, 0.35, 0.21, 1))
P.setdefault('lorry', (0.30, 0.32, 0.30, 1))

OPEL = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/SK_Ger_Opel.fbx'
BED_Z = 1.18                      # bed floor above ground
BOW_Z = 2.53                      # canvas bows across the bed
SLOTS = dict(front=-0.28, mid=-1.28, rear=-2.29)   # truck-local x of the gaps between the bows (measured)
TRUCK_LEN, TRUCK_W = 6.42, 2.44

SUB = 'Opel Blitz (real mesh) + soldier for scale  |  '


def setup():
    """Call first in build(): this Blender build's Workbench studio light renders about half as bright as the
    earlier Breakthrough rounds (sand ground 124 instead of 174 sRGB); one stop of exposure matches them."""
    bpy.context.scene.view_settings.exposure = 1.0


def rot(o, d, yaw):
    """World xy of local offset d from origin o turned by yaw (deg)."""
    a = math.radians(yaw)
    return (o[0] + d[0] * math.cos(a) - d[1] * math.sin(a), o[1] + d[0] * math.sin(a) + d[1] * math.cos(a))


# ------------------------------------------------------------------ real truck mesh
def _template(k, path, material):
    cache = k.__dict__.setdefault('fbx', {})
    key = (path, material.name)
    if key in cache:
        return cache[key]
    before = set(bpy.data.objects)
    units = bpy.context.scene.unit_settings
    keep = (units.system, units.scale_length, units.length_unit)
    bpy.ops.import_scene.fbx(filepath=path)
    # keep the scene's units as they were, whatever the FBX importer does
    units.system, units.scale_length, units.length_unit = keep
    new = [o for o in bpy.data.objects if o not in before]
    bpy.context.view_layer.update()
    parts = [(o.data, o.matrix_world.copy()) for o in new if o.type == 'MESH' and not o.name.startswith('UCX')]
    pts = [m @ v.co for me, m in parts for v in me.vertices]
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    base = Matrix.Translation((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]))
    for o in new:
        bpy.data.objects.remove(o)
    for me, _ in parts:
        me.materials.clear()
        me.materials.append(material)
    cache[key] = [(me, base @ m) for me, m in parts]
    return cache[key]


def real(k, path, loc, yaw=0, color='truck', cat='truck'):
    material = concept_kit._mat(color)
    place = Matrix.Translation(loc) @ Matrix.Rotation(math.radians(yaw), 4, 'Z')
    for me, m in _template(k, path, material):
        o = bpy.data.objects.new('real', me)
        bpy.context.scene.collection.objects.link(o)
        o.matrix_world = place @ m
        o['cat'] = cat


def truck(k, x, y, yaw, loads=None, label='Opel Blitz parks here (empty bed)'):
    """The real Opel at its parking transform (bottom centre x, y, heading yaw). loads = {slot: kind} already in
    the bed. Returns the world xy of the three bed slots."""
    real(k, OPEL, (x, y, 0), yaw)
    slots = {name: rot((x, y), (sx, 0), yaw) for name, sx in SLOTS.items()}
    for name, kind in (loads or {}).items():
        sx, sy = slots[name]
        unit_load(k, kind, (sx, sy, BED_Z), yaw, cat='truck')
    if label:
        fx, fy = rot((x, y), (1.6, 0), yaw)
        k.label(label, 'truck', (fx, fy, 2.6))
    return slots


# ------------------------------------------------------------------ unit loads (what the hook carries)
def unit_load(k, kind, loc, yaw=0, cat='building'):
    """One crane load, 0.75 m along the truck so it drops between two bed bows.
    radixite = steel skip of crystals, metal = strapped plate bundle on dunnage, parts = pallet of wheels + gearbox."""
    k.group(loc, yaw)
    if kind == 'radixite':
        k.box('skip', (0.75, 1.5, 0.5), (0, 0, 0), color='steel', cat=cat)
        for i, (ox, oy, s) in enumerate(((0, -0.4, 1.0), (0.1, 0.25, 0.8), (-0.15, 0.0, 0.9), (0.12, -0.05, 0.7),
                                          (-0.1, 0.55, 0.75))):
            k.cyl('crystal', 0.16 * s, 0.75 * s, (ox, oy, 0.25), color='radixite', r_top=0.0, segments=5,
                  rot=(14 * (i - 2), -10 * (i - 1), 40 * i), cat=cat)
    elif kind == 'metal':
        for oy in (-0.5, 0.5):
            k.box('dunnage', (0.7, 0.12, 0.1), (0, oy, 0), color='timber', cat=cat)
        k.box('plates', (0.72, 1.55, 0.4), (0, 0, 0.1), color='metal', cat=cat)
        for oy in (-0.45, 0.45):
            k.box('strap', (0.76, 0.06, 0.42), (0, oy, 0.09), color='cable', cat=cat)
    else:
        k.box('pallet', (0.75, 1.4, 0.12), (0, 0, 0), color='timber', cat=cat)
        k.cyl('wheels', 0.34, 0.45, (0, -0.32, 0.12), color='parts', segments=12, cat=cat)
        k.box('gearbox', (0.6, 0.55, 0.42), (0, 0.38, 0.12), color='parts', cat=cat)
        k.box('gear_hub', (0.25, 0.3, 0.2), (0, 0.38, 0.54), color='steel', cat=cat)
    k.end_group()


def hook(k, top, kind, load_z, yaw=0, falls=1):
    """Hoist rope from top (x, y, z) down to a hook block and the load hanging with its base at load_z."""
    x, y, z = top
    block_z = load_z + (0.75 if kind == 'radixite' else 0.6)
    k.beam((x, y, z), (x, y, block_z + 0.3), w=0.045, color='cable', name='rope')
    k.box('hook_block', (0.3, 0.3, 0.35), (x, y, block_z), color='crane')
    for sx, sy in ((0.3, 0.6), (-0.3, -0.6), (0.3, -0.6), (-0.3, 0.6)):
        dx, dy = rot((0, 0), (sx, sy), yaw)
        k.beam((x, y, block_z), (x + dx, y + dy, load_z + 0.45), w=0.025, color='cable', name='sling')
    unit_load(k, kind, (x, y, load_z), yaw)


# ------------------------------------------------------------------ stockpiles in the HQ vocabulary
def radixite_pile(k, loc, w=2.6, d=2.2, yaw=0, fill=1.0):
    k.group(loc, yaw)
    k.box('crystal_bed', (w * 0.92, d * 0.92, 0.18 + 0.25 * fill), (0, 0, 0), color='radixite', round_top=0.12,
          segments=2)
    spots = ((-0.30, 0.25, 1.0), (0.05, -0.05, 1.15), (0.32, 0.30, 0.85), (-0.32, -0.30, 0.8), (0.30, -0.30, 0.9),
             (0.0, 0.38, 0.7), (-0.05, -0.40, 0.65))
    for i, (fx, fy, s) in enumerate(spots[:max(2, round(len(spots) * fill))]):
        h = (0.9 + 0.7 * fill) * s
        k.cyl('crystal', 0.28 * s, h, (fx * w, fy * d, 0.2), color='radixite', r_top=0.0, segments=5,
              rot=(12 * ((i % 3) - 1), -9 * ((i % 2) * 2 - 1), 37 * i))
        k.cyl('crystal', 0.17 * s, h * 0.7, (fx * w + 0.25, fy * d - 0.15, 0.2), color='radixite', r_top=0.0,
              segments=5, rot=(-18, 14, 70 * i))
    k.end_group()


def metal_pile(k, loc, w=2.6, d=2.2, yaw=0, fill=1.0):
    k.group(loc, yaw)
    n = max(2, round(10 * fill))
    for i in range(n):                                   # plate stack
        k.box('plate', (w * 0.45, d * 0.42, 0.075), (-w * 0.24, d * 0.24, i * 0.08), color='metal',
              yaw=3 * ((i % 3) - 1))
    for i in range(max(1, n - 3)):
        k.box('plate', (w * 0.45, d * 0.42, 0.075), (-w * 0.24, -d * 0.24, i * 0.08), color='metal',
              yaw=-2 * ((i % 3) - 1))
    for layer in range(max(1, round(6 * fill))):        # pig-iron crib
        along = layer % 2 == 0
        for j in range(4):
            off = (j - 1.5) * 0.17
            loc2 = (w * 0.22, d * 0.2 + off, layer * 0.11) if along else (w * 0.22 + off, d * 0.2, layer * 0.11)
            k.box('pig', (0.6, 0.13, 0.1), loc2, color='metal', yaw=0 if along else 90)
    for j in range(max(1, round(3 * fill))):            # I-beams
        k.box('ibeam', (0.22, d * 0.85, 0.24), (w * 0.36 - j * 0.26, -d * 0.02, 0), color='metal')
        if fill > 0.6:
            k.box('ibeam', (0.22, d * 0.85, 0.24), (w * 0.36 - j * 0.26, -d * 0.02, 0.25), color='metal')
    k.end_group()


def parts_pile(k, loc, w=2.6, d=2.2, yaw=0, fill=1.0):
    k.group(loc, yaw)
    for (fx, fy), n in zip(((-0.32, 0.3), (-0.32, -0.05), (-0.32, -0.38)), (10, 8, 6)):
        m = max(1, round(n * fill))
        k.cyl('wheels', 0.36, m * 0.125, (fx * w, fy * d, 0), color='parts', segments=14)
        k.cyl('hub', 0.12, m * 0.125 + 0.04, (fx * w, fy * d, 0), color='steel', segments=8)
    for layer in range(max(1, round(5 * fill))):        # folded track
        k.box('track', (1.5, 0.52, 0.07), (w * 0.15, -d * 0.3, layer * 0.075), color='track',
              yaw=4 * ((layer % 2) * 2 - 1))
    k.box('gearbox', (0.9, 0.7, 0.5), (w * 0.17, d * 0.25, 0), color='parts')
    if fill > 0.5:
        k.box('gearbox', (0.62, 0.5, 0.36), (w * 0.15, d * 0.25, 0.5), color='parts')
        k.cyl('wheel_edge', 0.36, 0.12, (w * 0.42, -d * 0.05, 0.36), color='parts', rot=(90, 0, 0), segments=14)
    k.end_group()


PILES = dict(radixite=radixite_pile, metal=metal_pile, parts=parts_pile)
NAMES = dict(radixite='Radixite stock', metal='Metal stock', parts='Vehicle parts stock')


def stock(k, kind, loc, w=2.6, d=2.2, yaw=0, fill=1.0, label=None):
    PILES[kind](k, (loc[0], loc[1], loc[2] if len(loc) > 2 else 0.0), w, d, yaw, fill)
    k.label(label or NAMES[kind], kind, (loc[0], loc[1], (loc[2] if len(loc) > 2 else 0) + 1.1))


# ------------------------------------------------------------------ enclosures (neutral: timber, steel, sandbags)
def timber_bin(k, loc, w, d, yaw=0, h=0.8, open_side='front', floor=True):
    """Sleeper-walled bay on a plank floor; open_side 'front' (local -y), 'none' or 'both' (local -y and +y)."""
    k.group(loc, yaw)
    if floor:
        k.box('bin_floor', (w + 0.3, d + 0.3, 0.12), (0, 0, 0), color='wood')
    sides = [('back', (w + 0.3, 0.22, h), (0, d / 2 + 0.04, 0))]
    if open_side == 'none':
        sides.append(('front', (w + 0.3, 0.22, h * 0.6), (0, -d / 2 - 0.04, 0)))
    if open_side == 'both':
        sides = []
    for sgn in (-1, 1):
        sides.append(('side', (0.22, d + 0.3, h), (sgn * (w / 2 + 0.04), 0, 0)))
    for name, size, p in sides:
        k.box('bin_' + name, size, p, color='timber')
    for sx in (-1, 1):
        for sy in (-1, 1):
            if open_side == 'both' and False:
                continue
            k.box('bin_post', (0.18, 0.18, h + 0.25), (sx * (w / 2 + 0.06), sy * (d / 2 + 0.06), 0), color='timber')
    k.end_group()


def steel_bin(k, loc, w, d, yaw=0, h=0.6):
    """Low steel-plate tray on sleepers: all four sides closed, the crane picks from above."""
    k.group(loc, yaw)
    for i in range(3):
        k.box('sleeper', (0.25, d + 0.4, 0.14), ((i - 1) * w * 0.4, 0, 0), color='timber')
    k.box('tray_floor', (w, d, 0.06), (0, 0, 0.14), color='steel')
    for sgn in (-1, 1):
        k.box('tray_side', (w, 0.08, h), (0, sgn * d / 2, 0.14), color='steel')
        k.box('tray_end', (0.08, d, h), (sgn * w / 2, 0, 0.14), color='steel')
    k.end_group()


def sandbags(k, a, b, h=0.75, t=0.7):
    a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
    mid = (a + b) / 2
    yaw = math.degrees(math.atan2((b - a).y, (b - a).x))
    k.box('sandbag_wall', ((b - a).length, t, h), mid, yaw=yaw, color='sandbag', round_top=0.22, segments=3,
          taper=0.08)


def footing(k, loc, size=(0.9, 0.9), h=0.35):
    k.box('footing', (size[0], size[1], h), loc, color='concrete', round_top=0.06, segments=2)


# ------------------------------------------------------------------ flag, marks, scale
def flag(k, x, y, h=8.0, z=0.0, label='Capture flag (rises up the pole)'):
    """Pole with the flag raised at the top and a pale ghost of the lowered flag near the foot. The flag flies
    toward screen right so it reads broadside from the RTS camera."""
    k.box('flag_foot', (0.7, 0.7, 0.35), (x, y, z), color='concrete', round_top=0.06, segments=2)
    k.cyl('flag_pole', 0.075, h, (x, y, z + 0.3), color='frame', segments=10, cat='mast')
    k.cyl('flag_ball', 0.14, 0.22, (x, y, z + h + 0.3), color='frame', segments=10, cat='mast')
    yaw = 35.8
    for zz, col in ((z + h - 0.95, 'flag'), (z + 1.0, 'flag_ghost')):
        cx, cy = rot((x, y), (0.82, 0), yaw)
        k.box('flag_cloth', (1.55, 0.04, 1.0), (cx, cy, zz), color=col, yaw=yaw, cat='mast')
    k.label(label, 'flag', (x, y, z + h + 0.6))
    k.label('lowered / neutral', 'flag', (x, y, z + 1.6))


def arrow(k, x, y, yaw, label=None, length=3.0):
    k.group((x, y, 0.02), yaw)
    k.box('arrow_shaft', (length - 1.0, 0.5, 0.03), (-0.5, 0, 0), color='lane', cat='mark')
    k.poly('arrow_head', [(length / 2 - 1.0, -0.75), (length / 2, 0), (length / 2 - 1.0, 0.75)], 0.03,
           color='lane', cat='mark')
    k.end_group()
    if label:
        k.label(label, 'lane', (x, y, 0.4))


def apron(k, pts, z=0.0):
    k.poly('apron', pts, 0.02, z, color='apron', cat='mark')


def park(k, x, y, yaw, length=TRUCK_LEN + 0.6, width=TRUCK_W + 0.6):
    """Corner marks of the parking bay."""
    k.group((x, y, 0.025), yaw)
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * length / 2, sy * width / 2
            k.box('park', (0.9, 0.14, 0.02), (cx - sx * 0.45, cy, 0), color='lane', cat='mark')
            k.box('park', (0.14, 0.7, 0.02), (cx, cy - sy * 0.35, 0), color='lane', cat='mark')
    k.end_group()


def dashes(k, pts, color='crane', w=0.12, dash=0.5, gap=0.35, z=0.04):
    """Dashed ground line along points: crane travel paths, slew arcs."""
    for a, b in zip(pts, pts[1:]):
        a, b = Vector((a[0], a[1], z)), Vector((b[0], b[1], z))
        L = (b - a).length
        t = 0.0
        while t < L:
            t1 = min(L, t + dash)
            p0, p1 = a + (b - a) * (t / L), a + (b - a) * (t1 / L)
            o = k.box('dash', ((p1 - p0).length, w, 0.02), (p0 + p1) / 2, color=color, origin=(0, 0, 0), cat='mark')
            o.rotation_euler = (0, 0, math.atan2((b - a).y, (b - a).x))
            t = t1 + gap


def arc(c, r, a0, a1, n=24):
    return [(c[0] + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             c[1] + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def person(k, x, y, yaw=200):
    k.group((x, y, 0), yaw)
    for sy in (-0.11, 0.11):
        k.box('leg', (0.16, 0.15, 0.86), (0, sy, 0), color='scale', cat='scale')
    k.box('torso', (0.26, 0.44, 0.6), (0, 0, 0.84), color='scale', round_top=0.06, segments=2, cat='scale')
    k.cyl('head', 0.11, 0.25, (0, 0, 1.47), color='scale', segments=10, round_top=0.08, cat='scale')
    k.end_group()


def progress_bar(k, z, x=0.0, y=0.0):
    """Where the floating UI bar sits above the point: keep the silhouette below it."""
    k.box('ui_bar', (2.6, 0.3, 0.3), (x, y, z), color='ui', yaw=35.8, cat='mark')
    k.label('Progress bar (UI)', 'ui', (x, y, z + 0.3))


def drum(k, x, y):
    k.cyl('drum', 0.3, 0.9, (x, y, 0), color='lorry', segments=12)


def crates(k, x, y, yaw=0, n=3):
    for i in range(n):
        k.box('crate', (1.0, 0.7, 0.6), rot((x, y), (0, i * 0.75), yaw) + (0.0 if i < 2 else 0.6,), yaw=yaw,
              color='wood')


def lattice(k, base, h, w0, w1, color='crane', panels=5, member=0.12):
    """Square lattice tower from base (x, y, z): w0 wide at the foot, w1 at the top."""
    x, y, z = base
    corners = ((-1, -1), (1, -1), (1, 1), (-1, 1))

    def ring(t):
        w = (w0 + (w1 - w0) * t) / 2
        return [Vector((x + cx * w, y + cy * w, z + h * t)) for cx, cy in corners]

    rings = [ring(i / panels) for i in range(panels + 1)]
    for i in range(4):
        k.beam(rings[0][i], rings[-1][i], w=member * 1.4, color=color, name='leg')
    for r in rings[1:]:
        for i in range(4):
            k.beam(r[i], r[(i + 1) % 4], w=member, color=color, name='strut')
    for lo, hi in zip(rings, rings[1:]):
        for i in range(4):
            k.beam(lo[i], hi[(i + 1) % 4], w=member * 0.8, color=color, name='brace')
