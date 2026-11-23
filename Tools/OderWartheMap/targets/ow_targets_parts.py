"""Oder-Warthe Werk targets: geometry helpers and the concept pieces (Blender 5.x, background).

Same conventions as the OW kit (assets/ow_blender_lib.py): centimetres, +X = the side the piece faces, +Z up, pivot on
the ground. Blender +Y = Unreal -Y in the exported FBX. Every piece builder takes `destroyed` and returns a Builder.
Context meshes (mast, tunnel portal, a generic Werkeingang wall) are imported or built only for the renders.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'assets'))
import ow_blender_lib as L  # noqa: E402
from ow_blender_lib import Builder, rng, v3  # noqa: E402

# Target-only materials (instances of M_OW_Kit in Unreal, created by the target importer).
L.MATS.update({
    'MI_OW_CamoNet': dict(tex='T_OW_Earth_D', tint=(0.40, 0.44, 0.27), rough=0.95, metal=0.0, tile=250.0),
    'MI_OW_Sandbag': dict(tex='T_OW_Earth_D', tint=(0.78, 0.70, 0.52), rough=0.95, metal=0.0, tile=120.0),
    'MI_OW_Sod': dict(tex='T_OW_Earth_D', tint=(0.50, 0.55, 0.33), rough=0.95, metal=0.0, tile=400.0),
    'MI_OW_Burnt': dict(tex='T_OW_Concrete_D', tint=(0.07, 0.065, 0.06), rough=0.97, metal=0.0, tile=300.0),
    'MI_OW_Canvas': dict(tex='T_OW_Earth_D', tint=(0.55, 0.52, 0.38), rough=0.95, metal=0.0, tile=200.0),
    # stand-ins for the existing faction materials (the build script swaps in their real textures for renders)
    'MI_BF_SootedFortress_UVTile': dict(tex='T_OW_Concrete_D', tint=(0.6, 0.6, 0.58), rough=0.9, metal=0.0, tile=400.0),
    'M_BF_CamoNet': dict(tex='T_OW_Earth_D', tint=(0.5, 0.4, 0.3), rough=0.95, metal=0.0, tile=200.0),
    'M_GerSandBagScaffold_Atlas': dict(tex='T_OW_Earth_D', tint=(0.7, 0.65, 0.5), rough=0.95, metal=0.0, tile=100.0),
})

KIT_FBX = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/FBX/'


# ------------------------------------------------------------------------------------------------------ helpers
def hnoise(seed):
    r = rng(seed)
    waves = [(r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(0, 6.3), r.uniform(0.004, 0.02)) for _ in range(6)]

    def f(x, y):
        return sum(math.sin((ax * x + ay * y) * fr + ph) for ax, ay, ph, fr in waves) / 6.0
    return f


def ss(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def rot2(x, y, yaw):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    return x * c - y * s, x * s + y * c


def mound(b, cx, cy, rx, ry, h, mat, seed=3, amp=6.0, segs=22, rings=7, yaw=0.0, base=-25.0, crater=0.0,
          plateau=0.0):
    """Elliptical earth mound (smooth profile). crater > 0 sinks the top into a bowl. Returns the height function."""
    nz = hnoise(seed)

    def prof(t):          # t = normalised radius 0..1
        if t <= plateau:
            top = 1.0
        else:
            top = 1.0 - ss(plateau, 1.0, t)
        bowl = crater * (1.0 - ss(0.0, 0.55, t)) if crater else 0.0
        return base + (h - base) * top - bowl

    pts = []
    for i in range(rings + 1):
        t = i / rings
        ring = []
        for j in range(segs):
            a = 2 * math.pi * j / segs
            lx, ly = rx * t * math.cos(a), ry * t * math.sin(a)
            wx, wy = rot2(lx, ly, yaw)
            x, y = cx + wx, cy + wy
            z = prof(t) + (amp * nz(x, y) * min(1.0, t * 3) if i < rings else 0.0)
            ring.append(Vector((x, y, z)))
        pts.append(ring)
    t = L.MATS[mat]['tile']
    for i in range(rings):
        for j in range(segs):
            n = (j + 1) % segs
            if i == 0:
                q = [pts[0][j], pts[1][j], pts[1][n]]
            else:
                q = [pts[i][j], pts[i + 1][j], pts[i + 1][n], pts[i][n]]
            b.face(q, mat, [(p.x / t, p.y / t) for p in q], True)

    def height(x, y):
        lx, ly = rot2(x - cx, y - cy, -yaw)
        tt = math.sqrt((lx / rx) ** 2 + (ly / ry) ** 2)
        return prof(min(tt, 1.0)) if tt <= 1.0 else base
    return height


def chunks(b, r, n, cx, cy, rx, ry, hfun, size=(40, 150), mats=('MI_OW_Concrete', 'MI_OW_ConcreteDark'), rebar=0.25,
           sink=0.35, flat=0.5):
    """Concrete chunks scattered over a surface (hfun), some with bent rebar."""
    for _ in range(n):
        a = r.uniform(0, 2 * math.pi)
        d = math.sqrt(r.uniform(0, 1))
        x, y = cx + rx * d * math.cos(a), cy + ry * d * math.sin(a)
        sx = r.uniform(*size)
        sy = sx * r.uniform(0.5, 1.0)
        sz = sx * r.uniform(0.25, flat)
        z = hfun(x, y) + sz * (0.5 - sink)
        b.obox((x, y, z), (sx, sy, sz), r.uniform(0, 360), r.choice(mats), pitch_deg=r.uniform(-25, 25),
               roll_deg=r.uniform(-25, 25))
        if r.random() < rebar:
            p0 = Vector((x, y, z + sz * 0.3))
            d1 = Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(0.4, 1.2))).normalized()
            d2 = Vector((r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-0.2, 0.5))).normalized()
            p1 = p0 + d1 * r.uniform(50, 110)
            p2 = p1 + d2 * r.uniform(30, 70)
            b.tube([p0, p1, p2], [1.6, 1.6, 1.4], 4, 'MI_OW_RustSteel', smooth=False)


def rubble_heap(b, seed, cx, cy, rx, ry, h, n=28, yaw=0.0, earth='MI_OW_Earth', size=(50, 170), crater=0.0,
                mats=('MI_OW_Concrete', 'MI_OW_ConcreteDark')):
    hf = mound(b, cx, cy, rx, ry, h, earth, seed=seed, amp=14.0, yaw=yaw, crater=crater)
    chunks(b, rng(seed + 1), n, cx, cy, rx * 0.85, ry * 0.85, hf, size=size, mats=mats)
    return hf


def pole(b, x, y, z0, h, r=9.0, mat='MI_OW_Wood', tilt=(0.0, 0.0)):
    top = (x + tilt[0], y + tilt[1], z0 + h)
    b.tube([(x, y, z0 - 20), top], [r, r * 0.8], 6, mat, smooth=True)
    return top


def net(b, seed, x0, y0, x1, y1, z, sag=40.0, mat='MI_OW_CamoNet', res=8, burnt=False, droop=None):
    """Draped camouflage net over the rectangle (corners on poles at z). burnt: rags with holes, sagging low."""
    r = rng(seed)
    nz = hnoise(seed)
    nx, ny = res, max(2, int(res * abs(y1 - y0) / max(1.0, abs(x1 - x0))))
    G = {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            u, v = i / nx, j / ny
            x, y = x0 + (x1 - x0) * u, y0 + (y1 - y0) * v
            edge = min(u, 1 - u, v, 1 - v)
            zz = z - sag * (math.sin(math.pi * u) * math.sin(math.pi * v)) + 18 * nz(x * 2, y * 2)
            if edge == 0:       # ragged hanging edges
                zz -= r.uniform(10, 45)
                x += r.uniform(-25, 25)
                y += r.uniform(-25, 25)
            if burnt:
                zz = (zz - (droop or 0) * (math.sin(math.pi * u) * math.sin(math.pi * v)))
            G[i, j] = Vector((x, y, zz))
    for i in range(nx):
        for j in range(ny):
            if burnt and r.random() < 0.55:
                continue
            q = [G[i, j], G[i + 1, j], G[i + 1, j + 1], G[i, j + 1]]
            m = 'MI_OW_Burnt' if burnt and r.random() < 0.6 else mat
            b.face(q, m, None, True)
            b.face(q[::-1], m, None, True)        # two-sided for the sketches (UE: two-sided material)
            if not burnt and r.random() < 0.45:   # garland tufts
                c = (q[0] + q[2]) / 2
                b.obox((c.x, c.y, c.z + 6), (r.uniform(30, 60), r.uniform(20, 40), 14), r.uniform(0, 180), mat,
                       pitch_deg=r.uniform(-15, 15))


def net_on_poles(b, seed, x0, y0, x1, y1, z, destroyed=False, sag=40.0, mat='MI_OW_CamoNet'):
    r = rng(seed + 7)
    for k, (x, y) in enumerate(((x0, y0), (x1, y0), (x1, y1), (x0, y1))):
        if destroyed and k % 2 == 1:
            tl = (r.uniform(-1, 1) * 140, r.uniform(-1, 1) * 140)
            pole(b, x, y, 0, z * 0.85, r=6.0, tilt=tl, mat='MI_OW_Burnt')
        else:
            pole(b, x, y, 0, z, r=6.0)
    net(b, seed, x0, y0, x1, y1, z, sag=sag, mat=mat, burnt=destroyed, droop=z * 0.45 if destroyed else None)


def sandbags(b, seed, p0, p1, h, mat='MI_OW_Sandbag', scatter=0.0):
    r = rng(seed)
    p0, p1 = v3(tuple(p0) + (0,) * (3 - len(p0))), v3(tuple(p1) + (0,) * (3 - len(p1)))
    d = p1 - p0
    length = d.length
    yaw = math.degrees(math.atan2(d.y, d.x))
    rows = int(h / 16)
    for k in range(rows):
        n = int(length / 58)
        off = 0.5 if k % 2 else 0.0
        for i in range(n):
            t = (i + off + 0.5) / n
            if t > 1:
                continue
            c = p0 + d * t
            z = 8 + k * 16
            if scatter and r.random() < scatter * (k / max(1, rows - 1) + 0.3):
                # knocked off: lying on the ground beside the wall
                side = Vector((-d.y, d.x, 0)).normalized() * r.uniform(40, 180) * r.choice((-1, 1))
                b.obox((c.x + side.x, c.y + side.y, 8), (60, 32, 16), yaw + r.uniform(-60, 60), mat,
                       roll_deg=r.uniform(-10, 10))
                continue
            b.obox((c.x, c.y, z), (60 + r.uniform(-4, 4), 34, 16), yaw + r.uniform(-4, 4), mat,
                   roll_deg=r.uniform(-4, 4))


def door_plate(b, x, y, z0, w, h, yaw, mat='MI_OW_DarkSteel', t=12.0, pitch=0.0):
    c = Vector((x, y, z0 + h / 2))
    b.obox(c, (t, w, h), yaw, mat, pitch_deg=pitch)
    for zz in (0.2, 0.5, 0.8):
        o = Vector(rot2(t / 2 + 3, 0, yaw) + (0,))
        b.obox(c + o + Vector((0, 0, (zz - 0.5) * h)), (5, w - 20, 10), yaw, 'MI_OW_RustSteel', pitch_deg=pitch)


def void_quad(b, pts):
    b.face(pts, 'MI_OW_Void')


def burn_scar(b, seed, cx, cy, rx, ry, z=3.0):
    r = rng(seed)
    segs = 14
    ring = []
    for j in range(segs):
        a = 2 * math.pi * j / segs
        k = r.uniform(0.6, 1.15)
        ring.append((cx + rx * k * math.cos(a), cy + ry * k * math.sin(a), z))
    b.face(ring, 'MI_OW_Burnt')


# ------------------------------------------------------------------------------------------- context (renders only)
def import_kit(name, loc=(0, 0, 0)):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=KIT_FBX + name + '.fbx')
    objs = [o for o in bpy.data.objects if o not in before]
    for o in objs:
        if o.name.startswith('UCX_'):
            bpy.data.objects.remove(o)
            continue
        o.location = loc
    return [o for o in bpy.data.objects if o.name.startswith(name)]


def bind_kit_materials(obj):
    """Imported FBX slots come in named MI_OW_*; swap them for the lib's textured preview materials."""
    for i, m in enumerate(obj.data.materials):
        if m is None:
            continue
        base = m.name.split('.')[0]
        if base in L.MATS and not m.get('ow_preview'):
            m.name = base + '_fbx'
            mat = L.get_material(base)
            mat['ow_preview'] = 1
            obj.data.materials[i] = mat


def densify(obj, max_len=90.0, rounds=6):
    """Split long edges so a vertex deformation can bend flat faces (the portal's coping is one box)."""
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for _ in range(rounds):
        long_edges = [e for e in bm.edges if e.calc_length() > max_len]
        if not long_edges:
            break
        bmesh.ops.subdivide_edges(bm, edges=long_edges, cuts=1, use_grid_fill=False)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def werkeingang_wall(b):
    """Generic Werkeingang context: concrete face at x = 0 with a 1.4 x 2.2 m door, earth bank above and behind."""
    b.box((-420, -650, -30), (0, -70, 430), 'MI_OW_Concrete', skip=('-z',))
    b.box((-420, 70, -30), (0, 650, 430), 'MI_OW_Concrete', skip=('-z',))
    b.box((-420, -70, 220), (0, 70, 430), 'MI_OW_Concrete', skip=('-z',))
    void_quad(b, [(-60, -70, 0), (-60, 70, 0), (-60, 70, 220), (-60, -70, 220)])
    b.face([(-60, -70, 0), (0, -70, 0), (0, -70, 220), (-60, -70, 220)], 'MI_OW_ConcreteDark')
    b.face([(0, 70, 0), (-60, 70, 0), (-60, 70, 220), (0, 70, 220)], 'MI_OW_ConcreteDark')
    mound(b, -700, 0, 650, 1000, 600, 'MI_OW_Sod', seed=11, plateau=0.35)
