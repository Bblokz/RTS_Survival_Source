"""Oder-Warthe Werk targets: production meshes (Blender 5.x, background).

    Blender5.0.exe -b --factory-startup -P ow_targets_build.py -- <id> [<id> ...] [--preview]
ids: tunnel porch jammer railgate (or all). Picked concepts (2026-11-22): tunnel G (whole tunnel collapsed), command
entrance A (gas-lock porch on the Werkeingang, existing faction shaders), jammer F (umbrella wires on the Werk mast),
rail entrance A (armoured gate inside the tunnel portal).

Frames (Blender; the FBX turns Blender +Y into Unreal -Y, X and Z stay):
  tunnel   SM_OW_TunnelPortal's own frame: pivot = portal pivot, +X = the mouth.
  porch    the Werk's Entrance socket: +X = out of the Werkeingang door, door centre at y 0. Unreal: spawn at the
           Entrance socket location with yaw = the Werkeingang's yaw (7c 0, 8b 180, Peilturm 180).
  jammer   the Werk mast foot (SM_OW_WerkMast pivot, mast yaw 0): guy anchors at 90/210/330 deg (Blender).
  railgate the tunnel portal's frame, like the tunnel.
Outputs: CustomMapItems/OderWarthe/Targets/FBX/<name>.fbx (+ UCX hulls), Blend/, Renders/, targets_report.json.
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ow_targets_parts as P  # noqa: E402
from ow_targets_parts import L, Builder, rng, v3  # noqa: E402

ROOT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Targets/'
FBX = ROOT + 'FBX/'
BLEND = ROOT + 'Blend/'
RENDERS = ROOT + 'Renders/'
REPORT = ROOT + 'targets_report.json'
BF = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/'
BF_CONCRETE = BF + 'AssetsForFactionIdentity/Concrete/03_Sooted_Fortress/Textures/T_BF_Sooted_Fortress_{}.png'
BF_NET = BF + 'AssetsForFactionIdentity/CamoNet/Textures/T_CamoNet_B_Rotbraun_{}.png'
BF_SANDBAGS = BF + 'AssetsForFactionIdentity/Sandbags/'
PREVIEW = '--preview' in sys.argv
RESULTS = {}

# Existing game materials the porch and jammer use (bound by slot name in the importer).
CONCRETE_BF = 'MI_BF_SootedFortress_UVTile'      # the Werke's faction concrete, UVMap = 4 m tile
NET_BF = 'M_BF_CamoNet'                           # faction camo net, masked two-sided, 2 m tile
BAG = 'M_GerSandBagScaffold_Atlas'                # faction loose sandbag atlas
L.MATS[CONCRETE_BF] = dict(tex='T_OW_Concrete_D', tint=(0.6, 0.6, 0.58), rough=0.9, metal=0.0, tile=400.0)
L.MATS[NET_BF] = dict(tex='T_OW_Earth_D', tint=(0.5, 0.4, 0.3), rough=0.95, metal=0.0, tile=200.0)
L.MATS[BAG] = dict(tex='T_OW_Earth_D', tint=(0.7, 0.65, 0.5), rough=0.95, metal=0.0, tile=100.0)

OW = ['MI_OW_Concrete', 'MI_OW_ConcreteDark', 'MI_OW_Earth', 'MI_OW_RustSteel', 'MI_OW_DarkSteel', 'MI_OW_Void',
      'MI_OW_Wood']

# Portal numbers (assets/ow_build.py build_portal)
OPEN_W, OPEN_H, CHAM = 475.0, 520.0, 60.0
HW_X0, HW_X1 = 240.0, 300.0
WALL_TOP = 720.0
BACK_X = -350.0


# ================================================================================================ shared
def preview_materials():
    """Blender look of the faction materials (textures from the BF source folders) for the review renders."""
    sys.path.insert(0, BF + 'SharedParts')
    sys.path.insert(0, BF + 'HQ/Model')
    import hq_finish
    for name, make in ((CONCRETE_BF, lambda: hq_finish.pbr_material('tmp_c', BF_CONCRETE)),
                       (NET_BF, lambda: hq_finish.net_material(BF_NET)),
                       (BAG, lambda: hq_finish.pbr_sandbag(BF_SANDBAGS + 'T_Sn0Sn1Sn2Scaffold_Atlas_Diffuse.PNG'))):
        old = bpy.data.materials.get(name)
        new = make()
        if old is not None:
            old.user_remap(new)
            bpy.data.materials.remove(old)
        new.name = name


def merge_into(target, src):
    """Append src's mesh to target, mapping material slots by name."""
    names = [m.name.split('.')[0] for m in target.data.materials]
    remap = []
    for m in src.data.materials:
        base = m.name.split('.')[0]
        if base not in names:
            target.data.materials.append(m)
            names.append(base)
        remap.append(names.index(base))
    sm = src.data.copy()
    sm.polygons.foreach_set('material_index', [remap[p.material_index] for p in sm.polygons])
    sm.transform(src.matrix_world)
    bm = bmesh.new()
    bm.from_mesh(target.data)
    bm.from_mesh(sm)
    bm.to_mesh(target.data)
    bm.free()
    bpy.data.meshes.remove(sm)
    bpy.data.objects.remove(src)
    if target.data.has_custom_normals:
        bpy.context.view_layer.objects.active = target
        with bpy.context.temp_override(object=target, active_object=target):
            bpy.ops.mesh.customdata_custom_splitnormals_clear()
    target.data.set_sharp_from_angle(angle=math.radians(40))


def finish(name, obj, hulls, notes, pivot, facing):
    obj.name = obj.data.name = name
    info = L.export_fbx(obj, FBX + name + '.fbx', hulls)
    info.update(notes=notes, pivot=pivot, facing=facing)
    RESULTS[name] = info
    print('BUILT', name, json.dumps(info))
    return obj


def save_blend(tag):
    os.makedirs(BLEND, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND + 'OW_Target_%s.blend' % tag, compress=True)


def render(tag, target, distance, az, el=50.0, extra=()):
    if not PREVIEW:
        return
    preview_materials()
    for o in bpy.context.scene.objects:
        if o.type == 'MESH':
            P.bind_kit_materials(o)
    L.render_preview(RENDERS + '%s_az%d.png' % (tag, az), target=target, distance=distance, azimuth_deg=az,
                     elevation_deg=el, resolution=(1100, 760), samples=20, ground_z=-3.0,
                     ground_rgb=(0.17, 0.17, 0.10))


def bag_source():
    """The faction's loose sandbag (60 x 34 x 17 cm at 0.58), atlas UVs in its UVMap; returns (mesh data, along_x)."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=BF_SANDBAGS + 'SM_Ger_LooseSandBag_2.FBX')
    src = None
    for o in set(bpy.data.objects) - before:
        if o.type == 'MESH' and not o.name.startswith('UCX'):
            src = o
        else:
            bpy.data.objects.remove(o)
    me = src.data.copy()
    me.transform(src.matrix_world)
    vs = [v.co for v in me.vertices]
    lo = Vector([min(c[a] for c in vs) for a in range(3)])
    hi = Vector([max(c[a] for c in vs) for a in range(3)])
    me.transform(Matrix.Translation(-Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))))
    along_x = (hi.x - lo.x) > (hi.y - lo.y)
    me['long'] = max(hi.x - lo.x, hi.y - lo.y)
    me['tall'] = hi.z - lo.z
    me.uv_layers['LightMapUV'].name = 'UVMap_atlas'
    for layer in [u for u in me.uv_layers if u.name != 'UVMap_atlas']:
        me.uv_layers.remove(layer)
    me.uv_layers['UVMap_atlas'].name = 'UVMap'
    bpy.data.objects.remove(src)
    return me, along_x


def bag_wall(seed, runs, courses, scatter=0.0, scale=0.58, base=0.0):
    """Loose bags laid in running bond along runs [((x0, y0), (x1, y1))]; scatter knocks bags down beside the wall.
    Returns one object with the atlas material."""
    r = rng(seed)
    me, along_x = bag_source()
    length = me['long'] * scale
    course = me['tall'] * scale * 0.88
    bm = bmesh.new()
    for (a, b) in runs:
        a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
        d = b - a
        yaw = math.atan2(d.y, d.x) + (0 if along_x else math.pi / 2)
        n = max(1, int(d.length / (length * 0.95)))
        side = Vector((-d.y, d.x, 0)).normalized()
        for k in range(courses):
            for i in range(n):
                t = (i + 0.5 * (k % 2) + 0.5) / n
                if t > 1.0:
                    continue
                p = a + d * t
                z = base + k * course
                yy = yaw + r.uniform(-0.06, 0.06)
                if scatter and r.random() < scatter * (0.3 + k / max(1, courses - 1)):
                    p = p + side * r.uniform(40, 200) * r.choice((-1, 1))
                    z = max(base, -5.0)
                    yy += r.uniform(-1.2, 1.2)
                m = (Matrix.Translation((p.x, p.y, z)) @ Matrix.Rotation(yy, 4, 'Z') @
                     Matrix.Rotation(r.uniform(-0.05, 0.05), 4, 'X') @ Matrix.Scale(scale, 4))
                tmp = me.copy()
                tmp.transform(m)
                bm.from_mesh(tmp)
                bpy.data.meshes.remove(tmp)
    out = bpy.data.meshes.new('bags')
    bm.to_mesh(out)
    bm.free()
    out.materials.append(L.get_material(BAG))
    out.polygons.foreach_set('material_index', [0] * len(out.polygons))
    obj = bpy.data.objects.new('bags', out)
    bpy.context.scene.collection.objects.link(obj)
    bpy.data.meshes.remove(me)
    if 'LightMapUV' not in out.uv_layers:
        lm = out.uv_layers.new(name='LightMapUV')
        for i, d in enumerate(out.uv_layers['UVMap'].data):
            lm.data[i].uv = d.uv
    return obj


def drape_net(b, seed, x0, y0, x1, y1, z, sag, poles, torn=False, cell=45.0, droop=0.0):
    """Faction net (M_BF_CamoNet) draped over a rectangle: tied up at the pole tops, sagging between, hem pegged a
    little lower; UVs planar at the net's 2 m tile. torn: random cells missing and the sheet hanging low."""
    r = rng(seed)
    nz = P.hnoise(seed)
    nx = max(2, int(abs(x1 - x0) / cell))
    ny = max(2, int(abs(y1 - y0) / cell))
    G = {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            u, v = i / nx, j / ny
            x, y = x0 + (x1 - x0) * u, y0 + (y1 - y0) * v
            lift = max([math.exp(-((x - px) ** 2 + (y - py) ** 2) / (2 * 90.0 ** 2)) for px, py in poles] + [0.0])
            zz = z - sag * (1 - lift) - 12 * nz(x * 1.7, y * 1.7)
            edge = min(u, 1 - u, v, 1 - v)
            if edge < 1e-6:
                zz -= 30
            if torn:
                zz -= droop * (1 - lift) * r.uniform(0.7, 1.2)
            G[i, j] = Vector((x, y, max(zz, 4.0)))
    for i in range(nx):
        for j in range(ny):
            if torn and r.random() < 0.45:
                continue
            q = [G[i, j], G[i + 1, j], G[i + 1, j + 1], G[i, j + 1]]
            b.face(q, NET_BF, [(p.x / 200.0, p.y / 200.0) for p in q], True)


def door_leaf(b, c, w, h, yaw, pitch=0.0, t=10.0, mat='MI_OW_DarkSteel'):
    """Steel door leaf (centre c at mid height) with three straps and a handle on its outside (+local X)."""
    b.obox(c, (t, w, h), yaw, mat, pitch_deg=pitch)
    m = Matrix.Rotation(math.radians(yaw), 3, 'Z') @ Matrix.Rotation(math.radians(pitch), 3, 'Y')
    for f in (-0.35, 0.0, 0.35):
        b.obox(c + m @ Vector((t / 2 + 2, 0, f * h)), (4, w - 16, 9), yaw, 'MI_OW_RustSteel', pitch_deg=pitch)
    b.obox(c + m @ Vector((t / 2 + 4, w * 0.36, 0)), (6, 6, 26), yaw, 'MI_OW_RustSteel', pitch_deg=pitch)


# ================================================================================================ tunnel (G)
def portal_berm_h(x, y, nz):
    top = 772.0
    dxb = max(0.0, -450.0 - x)
    dy = max(0.0, abs(y) - 760.0)
    d = math.sqrt((dxb / 650.0) ** 2 + (dy / 340.0) ** 2)
    s = P.ss(0.0, 1.0, d)
    return (top + 35) * (1 - s) - 35 + 10 * nz(x, y) * (0.3 + s)


def trough_depth(x, y, nz):
    across = 1 - P.ss(0.0, 1.0, abs(y) / 700.0)
    along = 1 - 0.5 * P.ss(850.0, 1100.0, -x)
    return (430 + 60 * nz(x, y)) * across * along


def rot_yz(p, hy, hz, deg):
    a = math.radians(deg)
    dy, dz = p.y - hy, p.z - hz
    return Vector((p.x, hy + dy * math.cos(a) - dz * math.sin(a), hz + dy * math.sin(a) + dz * math.cos(a)))


def lintel_piece(p):
    """Rigid pieces of the broken lintel + coping (and the berm lip on them): left and right hinge down from the
    piers, the middle drops into the mouth and tips forward."""
    if not (HW_X0 - 15 <= p.x <= HW_X1 + 30 and p.z >= OPEN_H - 40 and abs(p.y) <= OPEN_W + 220):
        return None
    if p.y < -110:
        return rot_yz(p, -OPEN_W - 220, WALL_TOP + 40, -24)
    if p.y > 140:
        return rot_yz(p, OPEN_W + 220, WALL_TOP + 40, 19)
    c = Vector((280.0, 0.0, 620.0))
    a = math.radians(-20)
    d = p - c
    q = Vector((d.x * math.cos(a) + d.z * math.sin(a), d.y, -d.x * math.sin(a) + d.z * math.cos(a)))
    return c + q + Vector((140.0, 0.0, -330.0))


def tunnel_deform(p, nz):
    s = 1 if p.y > 0 else -1
    if p.x > HW_X1 - 4 and abs(p.x - HW_X1) > 0.6 and 22 < p.z < 560 and abs(p.y) < 745:   # door leaves: down
        return Vector((p.x + p.z * 0.8, p.y - s * p.z * 0.12, 14 + p.z * 0.06))
    q = lintel_piece(p)
    if q is not None:
        return q
    out = p.copy()
    if p.x < HW_X0 + 1 and p.z > 200:                                     # berm: trough along the tunnel
        out.z -= trough_depth(p.x, p.y, nz)
    if BACK_X - 5 <= p.x < HW_X0 - 20 and abs(p.y) <= OPEN_W + 5 and 60 < p.z <= OPEN_H + 5:   # lining crushed
        out.z -= p.z * 0.78
    if HW_X0 - 15 <= p.x <= HW_X1 + 30 and abs(p.y) <= 780 and p.z > 150:  # piers: cracked, leaning out a little
        out.x += 18 * (p.z / 720.0) + 10 * nz(p.y, p.z)
    if p.x > HW_X1 + 30 and abs(p.y) > 700 and p.z > 200:                 # wing wall tops crumbled
        out.z -= 95 * (0.55 + 0.45 * nz(p.x * 2, p.y)) * P.ss(200, 600, p.z)
    return out


def densify_region(obj, max_len, pred, rounds=6):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    for _ in range(rounds):
        es = [e for e in bm.edges if e.calc_length() > max_len and all(pred(v.co) for v in e.verts)]
        if not es:
            break
        bmesh.ops.subdivide_edges(bm, edges=es, cuts=1, use_grid_fill=False)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    bm.to_mesh(obj.data)
    bm.free()


def build_tunnel():
    L.reset_scene()
    obj = P.import_kit('SM_OW_TunnelPortal')[0]
    bpy.context.view_layer.update()
    nz = P.hnoise(77)          # the portal's own berm noise (ow_build.build_portal)
    densify_region(obj, 70.0, lambda c: (HW_X0 - 15 <= c.x <= HW_X1 + 30 and abs(c.y) <= 800) or
                   (c.x > HW_X1 + 20 and abs(c.y) > 690))
    for v in obj.data.vertices:
        v.co = tunnel_deform(v.co, nz)
    obj.data.update()
    b = Builder('rubble', OW)
    P.rubble_heap(b, 17, 430, 0, 700, 760, 470, n=70, size=(60, 220))
    P.rubble_heap(b, 18, 130, 0, 250, 520, 540, n=16)
    r = rng(19)
    for _ in range(16):                                   # the caved roof slabs in the trough
        x, y = r.uniform(-820, 170), r.uniform(-330, 330)
        z = portal_berm_h(x, y, nz) - trough_depth(x, y, nz) + 10
        b.obox((x, y, z), (r.uniform(170, 330), r.uniform(120, 250), 45), r.uniform(0, 180),
               r.choice(('MI_OW_Concrete', 'MI_OW_ConcreteDark')), pitch_deg=r.uniform(-28, 28),
               roll_deg=r.uniform(-28, 28))
    P.chunks(b, rng(20), 30, -330, 0, 520, 300,
             lambda x, y: portal_berm_h(x, y, nz) - trough_depth(x, y, nz), size=(40, 140))
    for k in range(3):                                    # coping fragments on the heap
        b.obox((560 + 90 * k, -260 + 230 * k, 300 - 60 * k), (130, 240, 45), 20 + 35 * k, 'MI_OW_Concrete',
               pitch_deg=-25 + 10 * k, roll_deg=12)
    merge_into(obj, b.to_object())
    # --- collision: berm sides, the trough, piers, wing walls, a plug over the mouth and apron (blocks every agent)
    xs = [x for x in sorted(set([-1100 + 75 * i for i in range(18)] + [HW_X0])) if x <= HW_X0]
    ys = [-1100 + 73.33 * i for i in range(31)]
    grid = [(x, y, portal_berm_h(x, y, nz) - trough_depth(x, y, nz)) for x in xs for y in ys]

    def region(fy):
        pts = [p for p in grid if fy(p[1])]
        return pts + [(p[0], p[1], -35) for p in pts]
    hulls = [region(lambda y: y <= -480), region(lambda y: y >= 480), region(lambda y: abs(y) <= 520)]
    for s in (1, -1):
        hulls.append(L.box_pts((HW_X0, s * OPEN_W, -35), (HW_X1 + 20, s * 760, 700)))
        T = 50.0
        wy = lambda x: 750.0 + 70.0 * (x - 300.0) / 650.0  # noqa: E731
        hulls.append([(x, s * (wy(x) + o), z) for x in (300.0, 950.0) for o in (0.0, T) for z in (-35, 600 - 0.7 * (x - 300))])
    hulls.append([(HW_X0, y, z) for y in (-OPEN_W, OPEN_W) for z in (-35, 560)] +
                 [(760, y, z) for y in (-700, 700) for z in (-35, 330)] + [(1090, y, -35) for y in (-560, 560)] +
                 [(1000, y, 60) for y in (-420, 420)])
    finish('SM_OW_TunnelPortal_Collapsed', obj, hulls,
           notes='Collapsed state of SM_OW_TunnelPortal (concept G, whole tunnel): the cut-and-cover roof caved in '
                 'along the tunnel (a trough of broken slabs through the berm), lintel and coping broken into three '
                 'pieces, wing wall tops crumbled, blast doors thrown down and crushed, mouth and apron packed with '
                 'concrete and earth. Same pivot, frame and kit materials as the portal. Collision: berm sides, '
                 'trough, piers, wing walls and a rubble plug over the mouth and apron (blocks every agent).',
           pivot='as SM_OW_TunnelPortal', facing='as SM_OW_TunnelPortal (+X = mouth)')
    save_blend('TunnelCollapsed')
    render('TunnelCollapsed', (150, 0, 250), 4300, 30)
    render('TunnelCollapsed', (-150, 0, 250), 4300, 155)


# ================================================================================================ porch (A)
# Entrance frame in Blender (Unreal y = -Blender y): Werkeingang face at x -80, the door around y 0 (1.4 x 2.2 m),
# splinter wall x 140..220 over y -210..200, its leg x -80..220 over y -290..-210, the alcove's open end at y +200.
FACE_X = -80.0
SPL_X0, SPL_X1 = 140.0, 220.0
LEG_Y0, LEG_Y1 = -290.0, -210.0
OPEN_Y = 200.0
ROOF_Z0, ROOF_Z1 = 262.0, 318.0


def porch_concrete(b, d):
    r = rng(31)
    C = CONCRETE_BF
    if not d:
        b.box((FACE_X, LEG_Y0 - 10, ROOF_Z0), (SPL_X1 + 25, OPEN_Y + 55, ROOF_Z1), C)          # roof slab
        b.box((FACE_X + 5, OPEN_Y, -90), (SPL_X1, OPEN_Y + 45, ROOF_Z0), C, skip=('+z',))       # closing wall
        P.void_quad(b, [(-20, OPEN_Y + 46, 0), (110, OPEN_Y + 46, 0), (110, OPEN_Y + 46, 205), (-20, OPEN_Y + 46, 205)])
        door_leaf(b, Vector((45, OPEN_Y + 56, 104)), 128, 206, 90)
        b.box((-32, OPEN_Y + 45, 205), (122, OPEN_Y + 58, 222), 'MI_OW_DarkSteel')
        for y in (LEG_Y0 + 40, -60, 110):                                                        # roof drains
            b.box((SPL_X1 + 25, y - 8, ROOF_Z0 + 12), (SPL_X1 + 45, y + 8, ROOF_Z0 + 22), 'MI_OW_RustSteel')
        P.mound(b, 70, -30, 175, 265, ROOF_Z1 + 75, 'MI_OW_Earth', seed=32, base=ROOF_Z1 - 6, plateau=0.1, amp=22.0)
        P.chunks(b, rng(37), 7, 70, -30, 120, 200, lambda x, y: ROOF_Z1 + 40, size=(25, 60), mats=('MI_OW_Earth',),
                 rebar=0.0)
        b.tube([(-40, LEG_Y0 + 30, ROOF_Z1), (-40, LEG_Y0 + 30, ROOF_Z1 + 120)], [9, 9], 8, 'MI_OW_DarkSteel')
        b.tube([(-40, LEG_Y0 + 30, ROOF_Z1 + 120), (-40, LEG_Y0 + 30, ROOF_Z1 + 135)], [22, 6], 8, 'MI_OW_DarkSteel')
        return
    # destroyed: the slab broke in two; the open-end half dropped into the alcove, the wall stands to waist height
    b.obox((70, -150, 250), (SPL_X1 + 25 - FACE_X, 260, 56), 0, C, roll_deg=-9, pitch_deg=4)
    b.obox((80, 120, 125), (SPL_X1 + 10 - FACE_X, 250, 56), 6, C, roll_deg=27, pitch_deg=-6)
    for k in range(6):                                                                           # jagged wall stub
        x0 = FACE_X + 5 + k * 50
        b.box((x0, OPEN_Y, -90), (x0 + 50, OPEN_Y + 45, r.uniform(60, 170)), C)
    door_leaf(b, Vector((300, 520, 8)), 128, 206, 60, pitch=88)
    hf = P.rubble_heap(b, 33, 60, 150, 240, 260, 150, n=24, earth='MI_OW_Earth', mats=(C,))
    P.chunks(b, rng(34), 14, 120, 330, 220, 200, lambda x, y: hf(x, y), size=(40, 120),
             mats=(C,))


def build_porch_variant(d):
    L.reset_scene()
    b = Builder('porch', [CONCRETE_BF, 'MI_OW_DarkSteel', 'MI_OW_RustSteel', 'MI_OW_Void', 'MI_OW_Earth', NET_BF])
    porch_concrete(b, d)
    poles = [(330, 560), (-60, 560)]
    for (px, py) in poles:
        if d and px > 0:
            b.tube([(px, py, -20), (px + 170, py + 120, 40)], [7, 6], 6, 'MI_OW_Wood')
            continue
        b.tube([(px, py, -90), (px, py, 250)], [7, 6], 6, 'MI_OW_Wood')
    drape_net(b, 35, FACE_X + 10, OPEN_Y - 40, 380, 600, 250, 70, poles + [(150, OPEN_Y + 30), (SPL_X1, OPEN_Y)],
              torn=d, droop=120)
    obj = b.to_object()
    runs = [((-60, 420), (250, 420)), ((260, 420), (260, 270))]
    merge_into(obj, bag_wall(36, runs, 10, scatter=0.55 if d else 0.0, base=-45.0))
    if not d:
        hulls = [L.box_pts((FACE_X, LEG_Y0 - 10, -20), (SPL_X1 + 25, OPEN_Y + 60, ROOF_Z1 + 40)),
                 L.box_pts((-70, 400, -10), (270, 440, 125)), L.box_pts((240, 260, -10), (280, 440, 125))]
    else:
        hulls = [L.box_pts((FACE_X, -260, -20), (SPL_X1 + 20, 0, 290)),
                 [(x, y, -20) for x in (-180, 300) for y in (-60, 410)] + [(60, 150, 170), (40, 90, 180)],
                 L.box_pts((FACE_X + 5, OPEN_Y, -20), (SPL_X1, OPEN_Y + 45, 120))]
    name = 'SM_OW_Target_CommandPorch' + ('_Destroyed' if d else '')
    finish(name, obj, hulls,
           notes=('Command bunker entrance, concept A (gas-lock porch): closes the Werkeingang alcove between the door '
                  'and its splinter wall with a roof slab and a wall with an armoured side door, earth on the roof, a '
                  'sandbag blast wall and a faction camo net over the forecourt. ' +
                  ('Destroyed: slab broken in two, one half dropped into the alcove, wall blown to waist height, '
                   'door thrown out, bags knocked down, net torn.' if d else '')),
           pivot='the Werk Entrance socket (0.8 m out of the Werkeingang door, door centre, ground)',
           facing='+X out of the door (Unreal); Blender -Y = Unreal +Y')
    return obj


def eingang_context():
    """The Werkeingang as built by werk_parts.eingang (7c sizes), in the Entrance frame, for the renders only."""
    b = Builder('ctx', [CONCRETE_BF, 'MI_OW_Void', 'MI_OW_Earth'])
    C = CONCRETE_BF
    b.box((FACE_X - 600, -420, -20), (FACE_X, 180, 420), C)               # block: Unreal y -1.8..4.2 m
    b.box((FACE_X - 620, -440, 415), (FACE_X + 40, 200, 490), C)           # eg_roof
    P.void_quad(b, [(FACE_X + 1, -70, 25), (FACE_X + 1, 70, 25), (FACE_X + 1, 70, 245), (FACE_X + 1, -70, 245)])
    b.box((SPL_X0, -210, -20), (SPL_X1, 200, 260), C)                        # splinter wall
    b.box((FACE_X, LEG_Y0, -20), (SPL_X1, LEG_Y1, 260), C)                   # its leg
    b.to_object().name = 'ctx'


def build_porch():
    for d in (False, True):
        build_porch_variant(d)
        tag = 'CommandPorch' + ('_Destroyed' if d else '')
        save_blend(tag)
        eingang_context()
        render(tag, (120, 160, 120), 2100, 40, el=55)


# ================================================================================================ jammer (F)
MAST_TOP = 1150.0
POST_R = 560.0
POST_H = 240.0
SECTORS = (30.0, 270.0)            # between the guy anchors (90/210/330); 150 deg hits the Peilturm's annex
CELLAR = Vector((0.0, -330.0, 0.0))


def post_points():
    for a in SECTORS:
        for da in (-21.0, -7.0, 7.0, 21.0):
            aa = math.radians(a + da)
            yield a, da, Vector((POST_R * math.cos(aa), POST_R * math.sin(aa), 0.0))


def wire(b, pts, r=1.3):
    b.tube(pts, [r] * len(pts), 3, 'MI_OW_DarkSteel', smooth=False, cap_start=False, cap_end=False)


def post(b, p, top=POST_H, lean=None):
    lean = lean or Vector((0, 0, 0))
    b.box((p.x - 24, p.y - 24, -18), (p.x + 24, p.y + 24, 14), 'MI_OW_ConcreteDark')
    t = Vector((p.x, p.y, top)) + lean
    b.tube([(p.x, p.y, -60), t], [8, 6.5], 6, 'MI_OW_Wood')
    b.tube([t, t + Vector((0, 0, 22))], [6, 6], 6, 'MI_OW_Concrete')       # insulator
    return t + Vector((0, 0, 22))


def cellar(b, d):
    c = CELLAR
    if not d:
        P.mound(b, c.x, c.y, 230, 170, 150, 'MI_OW_Earth', seed=41, plateau=0.45)
        b.box((c.x - 120, c.y + 115, -20), (c.x + 120, c.y + 140, 145), 'MI_OW_Concrete')
        P.void_quad(b, [(c.x - 45, c.y + 141, 0), (c.x + 45, c.y + 141, 0), (c.x + 45, c.y + 141, 180),
                        (c.x - 45, c.y + 141, 180)])
        door_leaf(b, Vector((c.x, c.y + 147, 92)), 92, 182, 90)
        b.tube([(c.x + 90, c.y - 40, 60), (c.x + 90, c.y - 40, 250)], [8, 8], 8, 'MI_OW_DarkSteel')
        b.box((c.x - 105, c.y + 140, 0), (c.x - 55, c.y + 175, 70), 'MI_OW_DarkSteel')          # tuning coil box
        wire(b, [(c.x - 80, c.y + 150, 70), (c.x - 60, c.y + 210, 140), (0, -40, 300)], r=2.2)  # downlead
        drape_net(b, 42, c.x - 260, c.y - 200, c.x + 260, c.y + 200, 200, 60,
                  [(c.x - 230, c.y - 170), (c.x + 230, c.y - 170), (c.x + 230, c.y + 170), (c.x - 230, c.y + 170)])
        for (px, py) in ((c.x - 230, c.y - 170), (c.x + 230, c.y - 170), (c.x + 230, c.y + 170), (c.x - 230, c.y + 170)):
            b.tube([(px, py, -15), (px, py, 200)], [5, 4], 6, 'MI_OW_Wood')
        return
    P.mound(b, c.x, c.y, 240, 180, 70, 'MI_OW_Earth', seed=41, crater=110)
    P.chunks(b, rng(43), 16, c.x, c.y, 200, 150, lambda x, y: 20, size=(40, 120))
    b.box((c.x - 120, c.y + 115, -20), (c.x - 20, c.y + 140, 70), 'MI_OW_Concrete')
    b.obox((c.x + 60, c.y + 160, 40), (25, 130, 90), 30, 'MI_OW_Concrete', roll_deg=60)
    door_leaf(b, Vector((c.x + 210, c.y + 260, 6)), 92, 182, 50, pitch=88)
    drape_net(b, 44, c.x - 260, c.y - 200, c.x + 260, c.y + 200, 120, 40, [(c.x - 230, c.y - 170)], torn=True,
              droop=60)
    b.tube([(c.x - 230, c.y - 170, -15), (c.x - 230, c.y - 170, 200)], [5, 4], 6, 'MI_OW_Wood')
    b.tube([(c.x + 230, c.y + 170, -15), (c.x + 330, c.y + 300, 20)], [5, 4], 6, 'MI_OW_Wood')


def build_jammer_variant(d):
    L.reset_scene()
    b = Builder('jammer', ['MI_OW_Concrete', 'MI_OW_ConcreteDark', 'MI_OW_DarkSteel', 'MI_OW_RustSteel', 'MI_OW_Wood',
                           'MI_OW_Earth', 'MI_OW_Void', NET_BF])
    r = rng(45)
    top = Vector((0, 0, MAST_TOP))
    if not d:
        b.tube([(0, 0, MAST_TOP - 30), (0, 0, MAST_TOP + 10)], [52, 52], 10, 'MI_OW_Concrete', cap_start=True)
    else:
        b.obox((40, 0, MAST_TOP - 420), (104, 30, 40), 0, 'MI_OW_Concrete', pitch_deg=50)
    for a, da, p in post_points():
        if not d:
            t = post(b, p)
            wire(b, [top + (p - Vector((0, 0, 0))).normalized() * 50, t])
            continue
        if abs(da) < 10.0 or r.random() < 0.3:            # toppled: lies on the ground pointing outwards
            out = p.normalized()
            b.box((p.x - 24, p.y - 24, -18), (p.x + 24, p.y + 24, 14), 'MI_OW_ConcreteDark')
            t = Vector((p.x, p.y, 10)) + out * POST_H
            b.tube([(p.x, p.y, 15), t], [8, 6.5], 6, 'MI_OW_Wood')
            wire(b, [top + p.normalized() * 50, (p * 0.55).to_3d() + Vector((0, 0, 260)), t])
        else:                                             # leaning, wire slack to the ground
            lean = Vector((r.uniform(-60, 60), r.uniform(-60, 60), -30))
            t = post(b, p, lean=lean)
            mid = p * 0.5 + Vector((r.uniform(-40, 40), r.uniform(-40, 40), 6))
            wire(b, [t, mid, mid * 0.4 + Vector((0, 0, 3))])
    cellar(b, d)
    obj = b.to_object()
    c = CELLAR
    hulls = ([L.box_pts((c.x - 200, c.y - 150, -20), (c.x + 200, c.y + 150, 150)),
              L.box_pts((c.x - 120, c.y + 110, -20), (c.x + 120, c.y + 160, 190))] if not d else
             [L.box_pts((c.x - 200, c.y - 150, -20), (c.x + 200, c.y + 150, 70))])
    if not d:   # the collar: something to hit from below when the cellar sits out of sight on a hilltop bench
        hulls.append(L.box_pts((-60, -60, MAST_TOP - 40), (60, 60, MAST_TOP + 15)))
    for a, da, p in post_points():
        hulls.append(L.box_pts((p.x - 24, p.y - 24, -18), (p.x + 24, p.y + 24, 14)))
    name = 'SM_OW_Target_Jammer' + ('_Destroyed' if d else '')
    finish(name, obj, hulls,
           notes=('Jammer transmitter, concept F (umbrella): a clamp collar near the Werk mast top, eight wires down '
                  'to short wooden posts with insulators in two of the three sectors between the guy anchors (the '
                  'third stays clear: the Peilturm tower annex stands there), and a sod-covered '
                  'tuning cellar with a downlead under a faction camo net. The mast itself is SM_OW_WerkMast and '
                  'stays. ' + ('Destroyed: collar slipped down, three or more posts toppled, wires slack, cellar caved, '
                               'net torn.' if d else '')),
           pivot='the Werk mast foot (SM_OW_WerkMast pivot)', facing='mast yaw (anchors at 90/210/330 deg, Blender)')
    return obj


def build_jammer():
    for d in (False, True):
        build_jammer_variant(d)
        tag = 'Jammer' + ('_Destroyed' if d else '')
        save_blend(tag)
        for o in P.import_kit('SM_OW_WerkMast'):
            o.name = 'ctx_mast'
        render(tag, (0, -100, 400), 3600, -60, el=48)


# ================================================================================================ rail gate (A)
GATE_X = 0.0         # gate front face, 3 m inside the portal face (the lining is lit concrete here)
GATE_H = 445.0


def build_railgate_variant(d):
    L.reset_scene()
    b = Builder('gate', ['MI_OW_DarkSteel', 'MI_OW_RustSteel', 'MI_OW_ConcreteDark', 'MI_OW_Concrete', 'MI_OW_Void'])
    r = rng(51)
    half = OPEN_W - 12
    for s in (1, -1):                                           # frame: two posts on the lining walls
        if d and s > 0:
            b.obox((GATE_X + 120, s * 300, 40), (40, 40, 470), 30, 'MI_OW_DarkSteel', pitch_deg=82)
            continue
        b.box((GATE_X - 30, s * half - 22, -5), (GATE_X + 10, s * half + 22, GATE_H + 30), 'MI_OW_DarkSteel')
    if not d:
        b.box((GATE_X - 30, -half - 22, GATE_H), (GATE_X + 10, half + 22, GATE_H + 30), 'MI_OW_DarkSteel')
        for s in (1, -1):                                       # two sliding leaves, slight overlap at the centre
            y0, y1 = (0, half) if s > 0 else (-half, 0)
            b.box((GATE_X - 10, y0 - 5, 4), (GATE_X + 14 + (6 if s > 0 else 0), y1 + 5, GATE_H - 6), 'MI_OW_DarkSteel')
            for k in range(1, 5):
                y = y0 + (y1 - y0) * k / 5
                b.box((GATE_X + 14, y - 6, 20), (GATE_X + 22 + (6 if s > 0 else 0), y + 6, GATE_H - 20),
                      'MI_OW_RustSteel')
            for z in (90, 220, 350):
                b.box((GATE_X + 14, y0 + 10, z - 6), (GATE_X + 22 + (6 if s > 0 else 0), y1 - 10, z + 6),
                      'MI_OW_RustSteel')
        P.void_quad(b, [(GATE_X + 29, -330, 10), (GATE_X + 29, -230, 10), (GATE_X + 29, -230, 200),
                        (GATE_X + 29, -330, 200)])                  # wicket door, open
        b.box((GATE_X + 20, -235, 10), (GATE_X + 120, -229, 200), 'MI_OW_DarkSteel')
        for y in (-72, 72):                                      # buffer stop beam over the rails
            b.box((GATE_X + 40, y - 10, 10), (GATE_X + 60, y + 10, 90), 'MI_OW_DarkSteel')
        b.box((GATE_X + 60, -140, 60), (GATE_X + 85, 140, 95), 'MI_OW_RustSteel')
        b.tube([(GATE_X + 40, 380, 300), (GATE_X + 60, 380, 300)], [16, 16], 8, 'MI_OW_RustSteel')
        hulls = [L.box_pts((GATE_X - 30, -half - 22, -5), (GATE_X + 90, half + 22, GATE_H + 30))]
    else:
        b.obox((GATE_X - 160, -230, 160), (26, half + 10, GATE_H - 30), 0, 'MI_OW_DarkSteel', pitch_deg=-38, roll_deg=5)
        b.obox((GATE_X + 60, 210, 150), (26, half - 120, GATE_H - 140), 25, 'MI_OW_DarkSteel', pitch_deg=12,
               roll_deg=-18)
        b.obox((GATE_X + 260, 330, 12), (26, 200, 150), 70, 'MI_OW_RustSteel', pitch_deg=86)
        b.obox((GATE_X + 90, 0, 40), (44, 2 * half, 30), 8, 'MI_OW_DarkSteel', pitch_deg=4, roll_deg=3)
        P.rubble_heap(b, 52, GATE_X - 60, 20, 220, 440, 140, n=22, earth='MI_OW_ConcreteDark', size=(40, 140))
        hulls = [L.box_pts((GATE_X - 280, -half, -5), (GATE_X + 120, half, 200))]
    obj = b.to_object()
    name = 'SM_OW_Target_RailGate' + ('_Destroyed' if d else '')
    finish(name, obj, hulls,
           notes=('Tunnel railway entrance, concept A: an armoured two-leaf sliding gate with frame, wicket door and '
                  'buffer beam across the tunnel 3 m inside the SM_OW_TunnelPortal mouth, over the rails. ' +
                  ('Destroyed: left leaf blown in, right leaf torn and bent, frame post and lintel down, debris.'
                   if d else '')),
           pivot='as SM_OW_TunnelPortal (spawn with the portal transform)', facing='+X = the mouth')
    return obj


def build_railgate():
    for d in (False, True):
        build_railgate_variant(d)
        tag = 'RailGate' + ('_Destroyed' if d else '')
        save_blend(tag)
        for o in P.import_kit('SM_OW_TunnelPortal'):
            o.name = 'ctx_portal'
        render(tag, (100, 0, 220), 2300, 20, el=32)


BUILDERS = dict(tunnel=build_tunnel, porch=build_porch, jammer=build_jammer, railgate=build_railgate)


def main():
    args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    ids = [a for a in args if not a.startswith('--')]
    if 'all' in ids:
        ids = list(BUILDERS)
    for i in ids:
        BUILDERS[i]()
    old = json.load(open(REPORT)) if os.path.exists(REPORT) else {}
    old.update(RESULTS)
    os.makedirs(ROOT, exist_ok=True)
    with open(REPORT, 'w') as f:
        json.dump(old, f, indent=1)
    print('OW_TARGETS_BUILD_DONE', list(RESULTS))


if __name__ == '__main__':
    main()
