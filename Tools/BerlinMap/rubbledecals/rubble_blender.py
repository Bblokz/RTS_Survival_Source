"""Renders the 16 rubble decal cells top-down in Blender (Cycles, orthographic, transparent film).

Run (Blender 4.0, headless):
  D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P rubble_blender.py -- --out <dir> [--cells 0,3] [--res 1024] [--samples 16]

Per cell it writes <out>/cellNN.npy, float32 (H, W, 9), image order (row 0 = top):
  0-2 albedo (sRGB numbers, premultiplied by coverage), 3 roughness (premult), 4 height in metres (premult),
  5-7 world normal (premult; +X = image right, +Y = image up), 8 coverage (alpha)
and <out>/cellNN_beauty.png (albedo over grey) for a quick look.

Pieces are real meshes: bricks, setts, slabs, tiles, shards are boxes cut by random planes (convex fragments), timber
and girders are boxes, rebar and pipes are bevelled curves, sheets are crumpled grids. Everything is placed in metres
inside a CELL_CM square; see rubble_table.py for the per-cell recipe.
"""
import math
import os
import random
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix, Vector, Euler, noise

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rubble_table as T  # noqa: E402

HALF = T.CELL_CM / 200.0          # 2.0 m half extent of a cell
EDGE = HALF - 0.13                # keep pieces this far inside (gutter for mips)


# ---------------------------------------------------------------- args
def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out, cells, res, samples = None, list(range(16)), T.RENDER_PX, 16
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out = argv[i + 1]; i += 2
        elif argv[i] == "--cells":
            cells = [int(c) for c in argv[i + 1].split(",")]; i += 2
        elif argv[i] == "--res":
            res = int(argv[i + 1]); i += 2
        elif argv[i] == "--samples":
            samples = int(argv[i + 1]); i += 2
        else:
            i += 1
    if not out:
        raise SystemExit("--out required")
    return out, cells, res, samples


# ---------------------------------------------------------------- scene
def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.curves, bpy.data.images):
        for item in list(coll):
            coll.remove(item)


def setup_render(res, samples, out_dir):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = False
    scene.cycles.max_bounces = 0
    scene.cycles.pixel_filter_type = "BLACKMAN_HARRIS"
    scene.cycles.filter_width = 1.2
    scene.render.resolution_x = res
    scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "Standard"
    scene.frame_set(1)

    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = HALF * 2.0
    cam_data.clip_start = 0.1
    cam_data.clip_end = 50.0
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (0.0, 0.0, 20.0)
    scene.collection.objects.link(cam)
    scene.camera = cam

    world = bpy.data.worlds.new("W")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.0
    scene.world = world

    vl = scene.view_layers[0]
    vl.use_pass_normal = True
    vl.use_pass_z = False
    for name, kind in (("alb", "COLOR"), ("rough", "VALUE"), ("hgt", "VALUE")):
        aov = vl.aovs.add()
        aov.name = name
        aov.type = kind

    scene.use_nodes = True
    tree = scene.node_tree
    for n in list(tree.nodes):
        tree.nodes.remove(n)
    rl = tree.nodes.new("CompositorNodeRLayers")
    comp = tree.nodes.new("CompositorNodeComposite")
    tree.links.new(rl.outputs["Image"], comp.inputs["Image"])
    fo = tree.nodes.new("CompositorNodeOutputFile")
    fo.base_path = out_dir
    fo.format.file_format = "OPEN_EXR"
    fo.format.color_depth = "32"
    fo.format.exr_codec = "ZIP"
    fo.file_slots.clear()
    for slot, sock in (("alb", "alb"), ("rough", "rough"), ("hgt", "hgt"), ("nrm", "Normal"), ("alpha", "Alpha"),
                       ("beauty", "Image")):
        fo.file_slots.new(slot)
        tree.links.new(rl.outputs[sock], fo.inputs[slot])
    return scene


# ---------------------------------------------------------------- materials
_mats = {}


def node(tree, kind, **props):
    n = tree.nodes.new(kind)
    for k, v in props.items():
        setattr(n, k, v)
    return n


def math_node(tree, op, a=None, b=None, c=None):
    n = tree.nodes.new("ShaderNodeMath")
    n.operation = op
    for i, v in enumerate((a, b, c)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[i].default_value = v
        else:
            tree.links.new(v, n.inputs[i])
    return n.outputs[0]


def mix_rgb(tree, blend, fac, a, b):
    n = tree.nodes.new("ShaderNodeMixRGB")
    n.blend_type = blend
    for sock, v in ((n.inputs[0], fac), (n.inputs[1], a), (n.inputs[2], b)):
        if isinstance(v, (int, float)):
            sock.default_value = v
        elif isinstance(v, tuple):
            sock.default_value = (v[0], v[1], v[2], 1.0)
        else:
            tree.links.new(v, sock)
    return n.outputs[0]


def get_material(key, dust_col):
    if key in _mats:
        return _mats[key]
    col_a, col_b, share_b, rough, dust, var = T.MATERIALS[key]
    mat = bpy.data.materials.new("M_" + key)
    mat.use_nodes = True
    tree = mat.node_tree
    for n in list(tree.nodes):
        tree.nodes.remove(n)
    out = node(tree, "ShaderNodeOutputMaterial")
    emit = node(tree, "ShaderNodeEmission")
    tree.links.new(emit.outputs[0], out.inputs["Surface"])

    info = node(tree, "ShaderNodeObjectInfo")
    rnd = info.outputs["Random"]
    rnd2 = math_node(tree, "FRACT", math_node(tree, "MULTIPLY", rnd, 13.37))
    sel_b = math_node(tree, "LESS_THAN", rnd2, share_b)
    base = mix_rgb(tree, "MIX", sel_b, tuple(col_a), tuple(col_b))
    bright = math_node(tree, "MULTIPLY_ADD", rnd, 2.0 * var, 1.0 - var)
    col = mix_rgb(tree, "MULTIPLY", 1.0, base, bright)

    tc = node(tree, "ShaderNodeTexCoord")
    nz = node(tree, "ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 22.0
    nz.inputs["Detail"].default_value = 8.0
    nz.inputs["Roughness"].default_value = 0.6
    tree.links.new(tc.outputs["Object"], nz.inputs["Vector"])
    mr = node(tree, "ShaderNodeMapRange")
    mr.inputs[1].default_value = 0.3
    mr.inputs[2].default_value = 0.7
    mr.inputs[3].default_value = 0.8
    mr.inputs[4].default_value = 1.18
    tree.links.new(nz.outputs["Fac"], mr.inputs[0])
    col = mix_rgb(tree, "MULTIPLY", 1.0, col, mr.outputs[0])

    if key in ("wood_charred", "charcoal", "beam"):
        # alligator char: dark cracks on a slightly lighter crust
        vor = node(tree, "ShaderNodeTexVoronoi")
        vor.feature = "DISTANCE_TO_EDGE"
        vor.inputs["Scale"].default_value = 26.0
        tree.links.new(tc.outputs["Object"], vor.inputs["Vector"])
        crack = math_node(tree, "LESS_THAN", vor.outputs["Distance"], 0.06)
        col = mix_rgb(tree, "MIX", crack, col, (0.015, 0.014, 0.013))

    # dust settles on the upward faces, in patches
    geo = node(tree, "ShaderNodeNewGeometry")
    sep = node(tree, "ShaderNodeSeparateXYZ")
    tree.links.new(geo.outputs["Normal"], sep.inputs[0])
    up = node(tree, "ShaderNodeMapRange")
    up.inputs[1].default_value = 0.35
    up.inputs[2].default_value = 1.0
    tree.links.new(sep.outputs["Z"], up.inputs[0])
    nz2 = node(tree, "ShaderNodeTexNoise")
    nz2.inputs["Scale"].default_value = 7.0
    nz2.inputs["Detail"].default_value = 4.0
    tree.links.new(tc.outputs["Object"], nz2.inputs["Vector"])
    patch = node(tree, "ShaderNodeMapRange")
    patch.inputs[1].default_value = 0.42
    patch.inputs[2].default_value = 0.68
    tree.links.new(nz2.outputs["Fac"], patch.inputs[0])
    dust_amt = math_node(tree, "MULTIPLY", math_node(tree, "MULTIPLY", up.outputs[0], patch.outputs[0]),
                         dust * (0.6 + 0.8 * 0.5))
    dust_amt = math_node(tree, "MULTIPLY", dust_amt, math_node(tree, "MULTIPLY_ADD", rnd2, 0.8, 0.6))
    col = mix_rgb(tree, "MIX", dust_amt, col, tuple(dust_col))

    tree.links.new(col, emit.inputs["Color"])
    aov_alb = node(tree, "ShaderNodeOutputAOV")
    aov_alb.name = "alb"
    tree.links.new(col, aov_alb.inputs["Color"])

    rough_v = math_node(tree, "MULTIPLY_ADD", nz.outputs["Fac"], 0.16, rough - 0.08)
    rough_v = math_node(tree, "ADD", rough_v, math_node(tree, "MULTIPLY", dust_amt, 0.3))
    aov_r = node(tree, "ShaderNodeOutputAOV")
    aov_r.name = "rough"
    tree.links.new(math_node(tree, "MINIMUM", rough_v, 1.0), aov_r.inputs["Value"])

    sep_p = node(tree, "ShaderNodeSeparateXYZ")
    tree.links.new(geo.outputs["Position"], sep_p.inputs[0])
    aov_h = node(tree, "ShaderNodeOutputAOV")
    aov_h.name = "hgt"
    tree.links.new(sep_p.outputs["Z"], aov_h.inputs["Value"])
    _mats[key] = mat
    return mat


# ---------------------------------------------------------------- meshes
def bm_box(sx, sy, sz):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz))
    return bm


def cut(bm, rng, n_cuts, keep=(0.45, 0.95), vertical=False, tilt=1.0):
    """Cut the convex mesh with n random planes (clear the outer side, cap the hole)."""
    for _ in range(n_cuts):
        if vertical:
            a = rng.uniform(0, 2 * math.pi)
            n = Vector((math.cos(a), math.sin(a), rng.uniform(-0.25, 0.25) * tilt))
        else:
            n = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1) * tilt))
        if n.length < 1e-4:
            continue
        n.normalize()
        support = max(v.co.dot(n) for v in bm.verts)
        low = min(v.co.dot(n) for v in bm.verts)
        d = low + (support - low) * rng.uniform(*keep)
        geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
        res = bmesh.ops.bisect_plane(bm, geom=geom, dist=1e-5, plane_co=n * d, plane_no=n, clear_outer=True)
        edges = [e for e in bm.edges if not e.is_manifold]
        if edges:
            bmesh.ops.holes_fill(bm, edges=edges, sides=0)
    return bm


def roughen(bm, rng, amp, cuts=2, freq=25.0):
    if amp <= 0:
        return bm
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    off = Vector((rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100)))
    for v in bm.verts:
        v.co += v.normal * noise.fractal(v.co * freq + off, 1.0, 2.0, 3) * amp
    return bm


def bm_to_mesh(bm, name):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = False
    return me


def merge(bms):
    out = bmesh.new()
    for bm in bms:
        me = bpy.data.meshes.new("tmp")
        bm.to_mesh(me)
        out.from_mesh(me)
        bpy.data.meshes.remove(me)
        bm.free()
    return out


def translate(bm, dx, dy, dz):
    for v in bm.verts:
        v.co += Vector((dx, dy, dz))
    return bm


# Every builder returns (mesh, kind) where kind tells the placer how to scale the object:
#   "mult"  object scale = s (mesh at nominal size), "size" = mesh is unit size, object scale = s metres,
#   "len"   mesh is 1 m along X, object scale X = s metres.
def build_piece(ptype, rng, name):
    if ptype == "brick":
        bm = bm_box(0.25, 0.12, 0.065)
        if rng.random() < 0.6:
            cut(bm, rng, rng.randint(1, 2), keep=(0.85, 0.97))
        return bm_to_mesh(roughen(bm, rng, 0.0025, 1, 40.0), name), "mult"
    if ptype == "halfbrick":
        bm = bm_box(0.25, 0.12, 0.065)
        a = rng.uniform(-0.6, 0.6)
        n = Vector((1.0, a * 0.5, rng.uniform(-0.3, 0.3))).normalized()
        bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-5,
                               plane_co=Vector((rng.uniform(-0.04, 0.05), 0, 0)), plane_no=n, clear_outer=True)
        bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if not e.is_manifold], sides=0)
        cut(bm, rng, rng.randint(0, 2), keep=(0.8, 0.95))
        return bm_to_mesh(roughen(bm, rng, 0.003, 1, 40.0), name), "mult"
    if ptype == "brickclump":
        parts = []
        # a lump of wall: two bricks wide (stretcher + header course), one to three courses, broken off
        courses = rng.choice([1, 2, 2, 3])
        for c in range(courses):
            if c % 2 == 0:
                for k in range(2):
                    b = bm_box(0.25, 0.12, 0.065)
                    translate(b, 0.0, (k - 0.5) * 0.13, c * 0.077)
                    parts.append(b)
            else:
                for k in range(2):
                    b = bm_box(0.12, 0.25, 0.065)
                    translate(b, (k - 0.5) * 0.13, 0.0, c * 0.077)
                    parts.append(b)
            mortar = bm_box(0.26, 0.255, 0.012)
            translate(mortar, 0, 0, c * 0.077 + 0.038)
            parts.append(mortar)
        bm = merge(parts)
        cut(bm, rng, rng.randint(1, 3), keep=(0.55, 0.85), vertical=True)
        return bm_to_mesh(bm, name), "mult"
    if ptype in ("chunk", "pebble", "charcoal"):
        bm = bm_box(1.0, rng.uniform(0.6, 1.0), rng.uniform(0.35, 0.7))
        cut(bm, rng, rng.randint(5, 9), keep=(0.5, 0.92))
        amp = 0.03 if ptype != "pebble" else 0.0
        return bm_to_mesh(roughen(bm, rng, amp, 1, 3.0), name), "size"
    if ptype == "slabchunk":
        bm = bm_box(1.0, rng.uniform(0.5, 0.9), rng.uniform(0.18, 0.28))
        cut(bm, rng, rng.randint(3, 6), keep=(0.45, 0.9), vertical=True)
        cut(bm, rng, 1, keep=(0.75, 0.95))
        return bm_to_mesh(roughen(bm, rng, 0.012, 1, 4.0), name), "size"
    if ptype in ("flake", "tileshard", "slate", "glass", "slabshard", "paperbit"):
        thick = {"flake": 0.12, "tileshard": 0.10, "slate": 0.045, "glass": 0.035, "slabshard": 0.17,
                 "paperbit": 0.02}[ptype]
        bm = bm_box(1.0, rng.uniform(0.45, 1.0), thick)
        cut(bm, rng, rng.randint(2, 5), keep=(0.35, 0.85), vertical=True, tilt=0.0 if ptype == "glass" else 1.0)
        amp = {"flake": 0.012, "paperbit": 0.03}.get(ptype, 0.0)
        return bm_to_mesh(roughen(bm, rng, amp, 2, 6.0), name), "size"
    if ptype == "tile":
        bm = bm_box(0.38, 0.18, 0.014)
        # Biberschwanz: rounded tail at +X
        for a in (0.9, -0.9):
            n = Vector((1.0, a, 0)).normalized()
            bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-5,
                                   plane_co=Vector((0.17, 0.06 * (1 if a > 0 else -1), 0)), plane_no=n, clear_outer=True)
            bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if not e.is_manifold], sides=0)
        if rng.random() < 0.5:
            cut(bm, rng, 1, keep=(0.6, 0.9), vertical=True)
        return bm_to_mesh(bm, name), "mult"
    if ptype == "sett":
        bm = bm_box(rng.uniform(0.13, 0.17), rng.uniform(0.12, 0.15), rng.uniform(0.11, 0.14))
        cut(bm, rng, rng.randint(2, 5), keep=(0.88, 0.97))
        return bm_to_mesh(roughen(bm, rng, 0.004, 1, 30.0), name), "mult"
    if ptype == "slab":
        bm = bm_box(0.35, 0.35, 0.05)
        cut(bm, rng, rng.randint(0, 2), keep=(0.85, 0.97), vertical=True)
        return bm_to_mesh(bm, name), "mult"
    if ptype in ("plank", "lath", "beam"):
        w, t = {"plank": (0.16, 0.026), "lath": (0.045, 0.02), "beam": (0.18, 0.17)}[ptype]
        bm = bm_box(1.0, w, t)
        for sgn in (1, -1):
            n = Vector((sgn * 1.0, rng.uniform(-0.8, 0.8), rng.uniform(-0.4, 0.4))).normalized()
            bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-5,
                                   plane_co=Vector((sgn * rng.uniform(0.40, 0.49), 0, 0)), plane_no=n, clear_outer=True)
            bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if not e.is_manifold], sides=0)
        amp = 0.01 if ptype == "beam" else 0.002
        return bm_to_mesh(roughen(bm, rng, amp, 2, 12.0), name), "len"
    if ptype == "ashlar":
        bm = bm_box(1.0, rng.uniform(0.55, 0.75), rng.uniform(0.5, 0.65))
        cut(bm, rng, rng.randint(2, 4), keep=(0.55, 0.9))
        return bm_to_mesh(roughen(bm, rng, 0.01, 1, 5.0), name), "size"
    if ptype == "cornice":
        bm = bm_box(1.0, 0.28, 0.22)
        for co, no in (((0, 0.14, 0.05), (0, 1, 1)), ((0, 0.14, -0.06), (0, 1, -0.6)), ((0, -0.09, 0.11), (0, -0.5, 1))):
            bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-5,
                                   plane_co=Vector(co), plane_no=Vector(no).normalized(), clear_outer=True)
            bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if not e.is_manifold], sides=0)
        for sgn in (1, -1):
            n = Vector((sgn, rng.uniform(-0.6, 0.6), rng.uniform(-0.5, 0.5))).normalized()
            bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-5,
                                   plane_co=Vector((sgn * rng.uniform(0.38, 0.48), 0, 0)), plane_no=n, clear_outer=True)
            bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if not e.is_manifold], sides=0)
        return bm_to_mesh(roughen(bm, rng, 0.006, 1, 8.0), name), "len"
    if ptype == "girder":
        parts = [translate(bm_box(1.0, 0.10, 0.012), 0, 0, 0.068), translate(bm_box(1.0, 0.10, 0.012), 0, 0, -0.068),
                 bm_box(1.0, 0.009, 0.13)]
        bm = merge(parts)
        n = Vector((1, rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5))).normalized()
        bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-5,
                               plane_co=Vector((0.45, 0, 0)), plane_no=n, clear_outer=True)
        return bm_to_mesh(bm, name), "len"
    if ptype == "sheet":
        bm = bmesh.new()
        bmesh.ops.create_grid(bm, x_segments=14, y_segments=10, size=0.5)
        off = Vector((rng.uniform(0, 50), rng.uniform(0, 50), 0))
        fold_a = rng.uniform(0, math.pi)
        fold = Vector((math.cos(fold_a), math.sin(fold_a), 0))
        for v in bm.verts:
            v.co.y *= 0.62
            d = v.co.dot(fold)
            v.co.z = noise.fractal(v.co * 4.0 + off, 1.0, 2.0, 3) * 0.05 + max(0.0, d) * 0.35
        cut(bm, rng, rng.randint(0, 2), keep=(0.6, 0.9), vertical=True)
        return bm_to_mesh(bm, name), "len"
    raise ValueError(ptype)


def build_curve(ptype, rng, length, name):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = 0.008 if ptype == "rebar" else rng.uniform(0.018, 0.035)
    cu.bevel_resolution = 2 if ptype == "rebar" else 4
    cu.use_fill_caps = True
    sp = cu.splines.new("POLY" if ptype == "rebar" else "BEZIER")
    if ptype == "rebar":
        n = rng.randint(3, 6)
        sp.points.add(n - 1)
        a = rng.uniform(0, 2 * math.pi)
        p = Vector((0, 0, 0))
        for i in range(n):
            sp.points[i].co = (p.x, p.y, p.z, 1.0)
            a += rng.uniform(-0.9, 0.9)
            step = length / (n - 1)
            p = p + Vector((math.cos(a) * step, math.sin(a) * step, rng.uniform(-0.02, 0.06)))
    else:
        sp.bezier_points.add(1)
        a = rng.uniform(0, 2 * math.pi)
        p1 = Vector((math.cos(a) * length, math.sin(a) * length, 0))
        for bp, co in zip(sp.bezier_points, (Vector((0, 0, 0)), p1)):
            bp.co = co
            bp.handle_left_type = bp.handle_right_type = "AUTO"
    return cu


# ---------------------------------------------------------------- layouts (density in the cell, x right, y up, metres)
def fbm2(x, y, seed, freq=1.0, octaves=4):
    return noise.fractal(Vector((x * freq + seed * 17.1, y * freq - seed * 9.7, seed * 0.37)), 1.0, 2.0, octaves)


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def density(shape, x, y, seed):
    w = fbm2(x, y, seed, 0.9)
    if shape in ("radial", "scatter", "sett_patch"):
        cx, cy = (0.25, -0.1) if shape == "sett_patch" else (0.0, 0.0)
        ang = math.atan2(y - cy, x - cx)
        R = 1.45 * (1.0 + 0.22 * fbm2(math.cos(ang), math.sin(ang), seed + 3, 1.3, 3))
        r = math.hypot(x - cx, y - cy) / R
        if shape == "scatter":
            return smoothstep(1.05, 0.25, r) * (0.75 + 0.25 * w)
        return smoothstep(1.0, 0.0, r + 0.15 * w) ** 0.9
    if shape == "fan":
        top = EDGE
        L = 2.9 * (1.0 + 0.18 * fbm2(x, 0, seed + 5, 1.1, 3))
        t = (top - y) / L
        if t < 0 or t > 1.0:
            return 0.0
        half_w = (1.45 + 0.35 * t) * (1.0 + 0.12 * fbm2(0, y, seed + 7, 1.4, 3))
        side = smoothstep(half_w, half_w * 0.55, abs(x))
        return side * (1.0 - t) ** 1.25 * (0.85 + 0.15 * w)
    if shape == "strip":
        half_h = 0.55 * (1.0 + 0.45 * fbm2(x, 0, seed + 11, 0.9, 3))
        ends = smoothstep(1.85, 1.2 + 0.3 * fbm2(0, y, seed + 13), abs(x))
        return smoothstep(half_h, half_h * 0.2, abs(y)) * ends
    if shape == "corner":
        dx = x + EDGE          # distance from the left edge
        dy = EDGE - y          # distance from the top edge
        reach = 1.45 * (1.0 + 0.2 * fbm2(x, y, seed + 17, 0.8, 3))
        along = min(dx, dy)
        corner_bulge = smoothstep(2.6, 0.0, math.hypot(dx, dy))
        d = max(smoothstep(reach, 0.0, along) * (0.55 + 0.45 * corner_bulge), corner_bulge * 0.9)
        far = smoothstep(3.6, 2.6, max(dx, dy))
        return d * far
    raise ValueError(shape)


def heap_height(cell, d):
    return cell["heap"] * d ** 1.4


# ---------------------------------------------------------------- placement
def place_cell(ci, cell, rng):
    seed = ci * 101 + 7
    bed_col = cell["bed"][0]
    mix = cell["mix"]
    weights = [m[2] for m in mix]
    meshes = {}
    placed = 0
    attempts = 0
    coll = bpy.context.scene.collection

    def pick():
        r = rng.random() * sum(weights)
        for m in mix:
            r -= m[2]
            if r <= 0:
                return m
        return mix[-1]

    def get_mesh(ptype):
        lst = meshes.setdefault(ptype, [])
        if len(lst) < 10:
            me, kind = build_piece(ptype, rng, "%s_%d" % (ptype, len(lst)))
            lst.append((me, kind))
            return lst[-1]
        return lst[rng.randrange(len(lst))]

    def add_object(ptype, matkey, s, x, y, d, yaw=None, flat=False):
        mat = get_material(matkey, bed_col)
        tilt_max = 4.0 if flat else (8.0 + 22.0 * d)
        rot_extra = Euler((0, 0, 0))
        if ptype in ("rebar", "pipe"):
            length = s
            cu = build_curve(ptype, rng, length, "%s_%d" % (ptype, placed))
            cu.materials.append(mat)
            ob = bpy.data.objects.new(cu.name, cu)
            ob.location = (x, y, heap_height(cell, d) * rng.uniform(0.3, 1.0) + cu.bevel_depth)
            coll.objects.link(ob)
            return ob
        me, kind = get_mesh(ptype)
        ob = bpy.data.objects.new("%s_%d" % (ptype, placed), me)
        ob.active_material = None
        if len(me.materials) == 0:
            me.materials.append(None)
        ob.material_slots[0].link = "OBJECT"
        ob.material_slots[0].material = mat
        if kind == "mult":
            scale = Vector((s, s, s))
        elif kind == "size":
            scale = Vector((s, s, s))
        else:
            scale = Vector((s, 1.0, 1.0))
        if ptype in ("brick", "halfbrick", "sett") and not flat:
            r = rng.random()
            if r < 0.22:
                rot_extra = Euler((math.pi / 2, 0, 0))
            elif r < 0.30:
                rot_extra = Euler((0, math.pi / 2, 0))
        if ptype in ("chunk", "pebble", "charcoal", "ashlar", "slabchunk"):
            rot_extra = Euler((rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 0))
        if ptype == "sheet":
            scale = Vector((s, s, s))
        tilt = math.radians(tilt_max)
        rot = Euler((rng.uniform(-tilt, tilt), rng.uniform(-tilt, tilt),
                     rng.uniform(0, 2 * math.pi) if yaw is None else yaw))
        rmat = rot.to_matrix() @ rot_extra.to_matrix()
        m = Matrix.LocRotScale(Vector((0, 0, 0)), rmat.to_quaternion(), scale)
        zmin = min((m @ Vector(c)).z for c in [(v.co.x, v.co.y, v.co.z) for v in me.vertices])
        base = 0.0 if flat else heap_height(cell, d) * rng.uniform(0.25, 1.0)
        ob.matrix_world = Matrix.LocRotScale(Vector((x, y, base - zmin - (0.0 if not flat else 0.02))),
                                             rmat.to_quaternion(), scale)
        coll.objects.link(ob)
        return ob

    if cell["shape"] == "sett_patch":
        # the intact part of the carriageway: rows of setts on the left, broken up towards the hole
        rows_y = np.arange(-1.7, 1.75, 0.16)
        for ry in rows_y:
            off = rng.uniform(0, 0.15)
            xs = np.arange(-EDGE + off, 0.4, 0.165)
            for rx in xs:
                keep_edge = smoothstep(-0.2 - 0.5 * fbm2(rx, ry, seed + 21, 1.2, 3), -1.1, rx)
                ring = math.hypot(rx + 0.2, ry) < 1.95 + 0.25 * fbm2(rx, ry, seed + 23)
                if not ring or rng.random() > keep_edge:
                    continue
                add_object("sett", "granite", rng.uniform(0.95, 1.05), rx + rng.uniform(-0.01, 0.01),
                           ry + rng.uniform(-0.01, 0.01), 0.0, yaw=rng.uniform(-0.06, 0.06), flat=True)
                placed += 1

    target = cell["n"]
    while placed < target and attempts < target * 400:
        attempts += 1
        x = rng.uniform(-EDGE, EDGE)
        y = rng.uniform(-EDGE, EDGE)
        d = density(cell["shape"], x, y, seed)
        if d <= 0.0 or rng.random() > d ** 1.1:
            continue
        ptype, matkey, w, s0, s1 = pick()
        s = rng.uniform(s0, s1)
        if ptype in ("plank", "lath", "beam", "girder", "cornice", "sheet", "rebar", "pipe"):
            # long pieces must stay inside the cell
            if abs(x) + s * 0.5 > EDGE or abs(y) + s * 0.5 > EDGE:
                s = max(0.3, min(s, 2 * (EDGE - max(abs(x), abs(y)))))
                if s <= 0.3:
                    continue
        add_object(ptype, matkey, s, x, y, d)
        placed += 1
    return placed


# ---------------------------------------------------------------- export
def load_exr(path):
    img = bpy.data.images.load(path, check_existing=False)
    img.colorspace_settings.name = "Non-Color"
    w, h = img.size
    arr = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(arr)
    bpy.data.images.remove(img)
    return arr.reshape(h, w, 4)[::-1]


def render_cell(ci, out, res, samples):
    reset_scene()
    _mats.clear()
    cell = T.CELLS[ci]
    tmp = os.path.join(out, "exr_%02d" % ci)
    os.makedirs(tmp, exist_ok=True)
    for f in os.listdir(tmp):
        os.remove(os.path.join(tmp, f))
    setup_render(res, samples, tmp)
    rng = random.Random(9000 + ci * 31)
    n = place_cell(ci, cell, rng)
    print("cell %02d %s: %d pieces" % (ci, cell["name"], n), flush=True)
    bpy.ops.render.render(write_still=False)
    files = {f.split("0001")[0]: os.path.join(tmp, f) for f in os.listdir(tmp)}
    alb = load_exr(files["alb"])
    rough = load_exr(files["rough"])
    hgt = load_exr(files["hgt"])
    nrm = load_exr(files["nrm"])
    alpha = load_exr(files["alpha"])
    beauty = load_exr(files["beauty"])
    data = np.concatenate([alb[..., :3], rough[..., :1], hgt[..., :1], nrm[..., :3], alpha[..., :1]], axis=-1)
    np.save(os.path.join(out, "cell%02d.npy" % ci), data.astype(np.float32))
    # quick look: albedo over mid grey
    a = beauty[..., 3:4]
    rgb = beauty[..., :3] + (1 - a) * 0.5
    img = bpy.data.images.new("look", res, res)
    img.pixels.foreach_set(np.concatenate([rgb, np.ones_like(a)], -1)[::-1].ravel().astype(np.float32))
    img.filepath_raw = os.path.join(out, "cell%02d_beauty.png" % ci)
    img.file_format = "PNG"
    img.save()


def main():
    out, cells, res, samples = parse_args()
    os.makedirs(out, exist_ok=True)
    for ci in cells:
        render_cell(ci, out, res, samples)
    print("RUBBLE_RENDER_DONE", flush=True)


main()
