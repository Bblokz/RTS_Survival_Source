"""Builds the Molotov cocktail prop headless: model, UVs, baked textures, FBX, preview renders and the icon render.

Run:  Blender5.0.exe -b --factory-startup --python blender_build_molotov.py -- <out_dir>
Outputs in <out_dir>: SM_Molotov.fbx, T_Molotov_BaseColor.png, T_Molotov_Normal.png, T_Molotov_ORM.png,
preview_*.png, icon_render.png, molotov.blend.

Units: modelled at real size in metres, then scaled by PropScale (the project's grenades are ~2.2x real size so they
read from the RTS camera). Pivot at the bottle base, bottle along +Z like SM_M24StickGrenade.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector, noise

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else os.path.join(os.path.dirname(__file__), "out")
os.makedirs(OUT, exist_ok=True)

PROP_SCALE = 2.2
TEX = 1024
SEG = 28

# ---------------------------------------------------------------- scene reset
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0


def link(obj):
    scene.collection.objects.link(obj)
    return obj


def make_obj(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return link(bpy.data.objects.new(name, me))


# ---------------------------------------------------------------- bottle (lathe)
# (radius, z) in metres, outer wall from the bottom centre up to the lip.
BOTTLE_PROFILE = [
    (0.000, 0.002), (0.028, 0.000), (0.034, 0.003), (0.036, 0.010),
    (0.036, 0.060), (0.036, 0.120), (0.0358, 0.150), (0.034, 0.168),
    (0.029, 0.182), (0.021, 0.194), (0.0155, 0.204), (0.0140, 0.215),
    (0.0138, 0.240), (0.0150, 0.243), (0.0155, 0.250), (0.0148, 0.255),
    (0.0120, 0.256),
]


def lathe(name, profile, seg):
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        if r < 1e-5:
            rings.append([bm.verts.new((0, 0, z))])
            continue
        rings.append([bm.verts.new((r * math.cos(2 * math.pi * i / seg), r * math.sin(2 * math.pi * i / seg), z))
                      for i in range(seg)])
    for a, b in zip(rings[:-1], rings[1:]):
        if len(a) == 1:
            for i in range(seg):
                bm.faces.new((a[0], b[i], b[(i + 1) % seg]))
        else:
            for i in range(seg):
                bm.faces.new((a[i], a[(i + 1) % seg], b[(i + 1) % seg], b[i]))
    # Close the lip: the rag hides it.
    top = rings[-1]
    bm.faces.new(list(reversed(top)) if False else top)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return make_obj(name, bm)


bottle = lathe("Bottle", BOTTLE_PROFILE, SEG)

# ---------------------------------------------------------------- rag
# Plug stuffed into the neck plus a lumpy knot sticking out of the lip.
bm = bmesh.new()
bmesh.ops.create_icosphere(bm, subdivisions=3, radius=1.0)
for v in bm.verts:
    p = v.co.copy()
    # Lumpy knot: squash, offset to one side, noise.
    p.x *= 0.020; p.y *= 0.019; p.z *= 0.026
    n = noise.noise(Vector((v.co.x * 2.3, v.co.y * 2.3, v.co.z * 2.3)))
    p *= 1.0 + 0.22 * n
    p += Vector((0.002, 0.0, 0.268))
    v.co = p
knot = make_obj("RagKnot", bm)

# Flap of cloth hanging out of the neck down over the shoulder (strip following the neck surface).
bm = bmesh.new()
STRIP_LEN, STRIP_W = 16, 5
path = []
for i in range(STRIP_LEN + 1):
    t = i / STRIP_LEN
    z = 0.272 - t * 0.110            # from the knot down to the shoulder
    # radius follows the bottle profile plus a small gap, flaring out at the end
    if z > 0.204:
        r = 0.0175 + 0.004 * (1 - t)
    else:
        r = 0.0175 + (0.204 - z) * 0.62
    ang = math.radians(-20 + 35 * t)  # slight spiral
    path.append((r, z, ang))
grid = []
for i, (r, z, ang) in enumerate(path):
    row = []
    width = STRIP_W and (0.026 + 0.008 * math.sin(i * 0.9))
    for j in range(STRIP_W):
        s = (j / (STRIP_W - 1) - 0.5) * width
        a = ang + s / max(r, 0.01)
        wob = 0.0025 * noise.noise(Vector((i * 0.45, j * 0.6, 0.3)))
        rr = r + wob + 0.0015
        row.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), z + 0.002 * noise.noise(Vector((j, i, 1.7))))))
    grid.append(row)
for i in range(STRIP_LEN):
    for j in range(STRIP_W - 1):
        bm.faces.new((grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]))
flap = make_obj("RagFlap", bm)
sol = flap.modifiers.new("Solidify", 'SOLIDIFY')
sol.thickness = 0.0022
sol.offset = 1.0

# Twine wrapped twice around the neck to hold the rag.
twines = []
for k, z in enumerate((0.222, 0.229)):
    bpy.ops.mesh.primitive_torus_add(major_radius=0.0168 + 0.0012 * k, minor_radius=0.0016,
                                     major_segments=24, minor_segments=6, location=(0, 0, z))
    t = bpy.context.active_object
    t.name = f"Twine{k}"
    t.rotation_euler = (math.radians(4 - 8 * k), math.radians(3), 0)
    twines.append(t)

# ---------------------------------------------------------------- materials (procedural, baked later)

def new_mat(name):
    m = bpy.data.materials.new(name)
    try:
        m.use_nodes = True
    except Exception:
        pass
    return m


def obj_z(nt):
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(tc.outputs["Object"], sep.inputs[0])
    return tc, sep


def mix_rgb(nt, fac, a, b):
    mx = nt.nodes.new("ShaderNodeMix")
    mx.data_type = 'RGBA'
    nt.links.new(fac, mx.inputs[0]) if not isinstance(fac, float) else None
    if isinstance(fac, float):
        mx.inputs[0].default_value = fac
    for sock, val in ((mx.inputs[6], a), (mx.inputs[7], b)):
        if isinstance(val, tuple):
            sock.default_value = val
        else:
            nt.links.new(val, sock)
    return mx.outputs[2]


def math_node(nt, op, a, b=None, clamp=False):
    m = nt.nodes.new("ShaderNodeMath")
    m.operation = op
    m.use_clamp = clamp
    for idx, v in enumerate((a, b)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            m.inputs[idx].default_value = v
        else:
            nt.links.new(v, m.inputs[idx])
    return m.outputs[0]


def band(nt, z, lo, hi, soft=0.002):
    """1 inside [lo, hi] along z with a soft edge."""
    a = math_node(nt, 'SMOOTH_MIN' if False else 'GREATER_THAN', z, lo)
    b = math_node(nt, 'LESS_THAN', z, hi)
    return math_node(nt, 'MULTIPLY', a, b)


def noise_tex(nt, coord, scale, detail=4.0, rough=0.6):
    n = nt.nodes.new("ShaderNodeTexNoise")
    n.inputs["Scale"].default_value = scale
    n.inputs["Detail"].default_value = detail
    n.inputs["Roughness"].default_value = rough
    nt.links.new(coord, n.inputs["Vector"])
    return n


def glass_material():
    m = new_mat("MAT_Glass")
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tc, sep = obj_z(nt)
    z = sep.outputs["Z"]
    x = sep.outputs["X"]
    y = sep.outputs["Y"]
    nz = noise_tex(nt, tc.outputs["Object"], 60.0)
    # Fuel level: amber-tinted below, bottle green above.
    fuel = math_node(nt, 'LESS_THAN', math_node(nt, 'ADD', z, math_node(nt, 'MULTIPLY', nz.outputs["Fac"], 0.004)), 0.150)
    glass_col = mix_rgb(nt, nz.outputs["Fac"], (0.03, 0.08, 0.04, 1), (0.05, 0.11, 0.055, 1))
    fuel_col = mix_rgb(nt, nz.outputs["Fac"], (0.075, 0.075, 0.018, 1), (0.10, 0.09, 0.022, 1))
    col = mix_rgb(nt, fuel, glass_col, fuel_col)
    # Paper label band, front 3/4 of the bottle (angle from atan2).
    ang = math_node(nt, 'ARCTAN2', y, x)
    front = math_node(nt, 'GREATER_THAN', ang, -2.2)
    label = math_node(nt, 'MULTIPLY', band(nt, z, 0.068, 0.118), front)
    paper_n = noise_tex(nt, tc.outputs["Object"], 25.0, 6.0, 0.7)
    paper = mix_rgb(nt, paper_n.outputs["Fac"], (0.42, 0.36, 0.24, 1), (0.66, 0.58, 0.42, 1))
    # Red stripe + printed block (reads as a vodka label at RTS distance).
    stripe = band(nt, z, 0.104, 0.111)
    paper = mix_rgb(nt, stripe, paper, (0.42, 0.05, 0.035, 1))
    block = math_node(nt, 'MULTIPLY', band(nt, z, 0.078, 0.095), band(nt, ang, -0.9, 0.9))
    paper = mix_rgb(nt, math_node(nt, 'MULTIPLY', block, 0.85), paper, (0.10, 0.08, 0.06, 1))
    col = mix_rgb(nt, label, col, paper)
    # Grime towards the base.
    grime = math_node(nt, 'MULTIPLY', math_node(nt, 'LESS_THAN', z, 0.03), 0.5)
    col = mix_rgb(nt, grime, col, (0.08, 0.06, 0.04, 1))
    nt.links.new(col, bsdf.inputs["Base Color"])
    rough = math_node(nt, 'ADD', math_node(nt, 'MULTIPLY', label, 0.72), math_node(nt, 'ADD', 0.10, math_node(nt, 'MULTIPLY', grime, 0.6)))
    nt.links.new(rough, bsdf.inputs["Roughness"])
    # Tiny surface waviness on the glass, paper crinkle on the label.
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.25
    hn = math_node(nt, 'ADD', math_node(nt, 'MULTIPLY', nz.outputs["Fac"], 0.3), math_node(nt, 'MULTIPLY', paper_n.outputs["Fac"], label))
    nt.links.new(hn, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def cloth_material():
    m = new_mat("MAT_Cloth")
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tc, sep = obj_z(nt)
    z = sep.outputs["Z"]
    nz = noise_tex(nt, tc.outputs["Object"], 45.0, 5.0, 0.65)
    base = mix_rgb(nt, nz.outputs["Fac"], (0.27, 0.24, 0.18, 1), (0.45, 0.40, 0.31, 1))
    # Fuel soaked (darker, browner) towards the neck, sooty at the tip.
    soak = math_node(nt, 'MULTIPLY', math_node(nt, 'LESS_THAN', z, 0.245), 0.65)
    base = mix_rgb(nt, soak, base, (0.20, 0.15, 0.09, 1))
    soot_mask = math_node(nt, 'MULTIPLY', math_node(nt, 'GREATER_THAN', math_node(nt, 'ADD', z, math_node(nt, 'MULTIPLY', nz.outputs["Fac"], 0.02)), 0.283), 0.9)
    base = mix_rgb(nt, soot_mask, base, (0.04, 0.035, 0.03, 1))
    nt.links.new(base, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.95
    # Weave + wrinkles.
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.inputs["Scale"].default_value = 420.0
    wave.bands_direction = 'DIAGONAL'
    nt.links.new(tc.outputs["Object"], wave.inputs["Vector"])
    wr = noise_tex(nt, tc.outputs["Object"], 18.0, 3.0, 0.5)
    h = math_node(nt, 'ADD', math_node(nt, 'MULTIPLY', wave.outputs["Fac"], 0.04), wr.outputs["Fac"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.35
    nt.links.new(h, bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def twine_material():
    m = new_mat("MAT_Twine")
    nt = m.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tc = nt.nodes.new("ShaderNodeTexCoord")
    nz = noise_tex(nt, tc.outputs["Object"], 120.0)
    nt.links.new(mix_rgb(nt, nz.outputs["Fac"], (0.17, 0.11, 0.05, 1), (0.30, 0.20, 0.10, 1)), bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.9
    wave = nt.nodes.new("ShaderNodeTexWave")
    wave.inputs["Scale"].default_value = 300.0
    nt.links.new(tc.outputs["Object"], wave.inputs["Vector"])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.3
    nt.links.new(wave.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    return m


MAT_GLASS, MAT_CLOTH, MAT_TWINE = glass_material(), cloth_material(), twine_material()
bottle.data.materials.append(MAT_GLASS)
knot.data.materials.append(MAT_CLOTH)
flap.data.materials.append(MAT_CLOTH)
for t in twines:
    t.data.materials.append(MAT_TWINE)

# ---------------------------------------------------------------- join, scale, shade, UV
bpy.ops.object.select_all(action='DESELECT')
for o in [bottle, knot, flap] + twines:
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    for mod in list(o.modifiers):
        bpy.ops.object.modifier_apply(modifier=mod.name)
bpy.context.view_layer.objects.active = bottle
bpy.ops.object.join()
mol = bpy.context.active_object
mol.name = "SM_Molotov"
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.ops.object.shade_smooth_by_angle(angle=math.radians(70))

bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=0.02, scale_to_bounds=False)
bpy.ops.uv.pack_islands(margin=0.01, rotate=True)
bpy.ops.object.mode_set(mode='OBJECT')

# ---------------------------------------------------------------- bake
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 16
bake_imgs = {}


def bake(kind, colorspace, samples=16, **kw):
    img = bpy.data.images.new(f"bake_{kind}", TEX, TEX, alpha=False, float_buffer=False)
    img.colorspace_settings.name = colorspace
    for mat in (MAT_GLASS, MAT_CLOTH, MAT_TWINE):
        nt = mat.node_tree
        node = nt.nodes.get("BakeTarget") or nt.nodes.new("ShaderNodeTexImage")
        node.name = "BakeTarget"
        node.image = img
        for n in nt.nodes:
            n.select = False
        node.select = True
        nt.nodes.active = node
    scene.cycles.samples = samples
    bpy.ops.object.select_all(action='DESELECT')
    mol.select_set(True)
    bpy.context.view_layer.objects.active = mol
    scene.render.bake.margin = 8
    bpy.ops.object.bake(type=kind, **kw)
    bake_imgs[kind] = img
    return img


img_col = bake('DIFFUSE', 'sRGB', 1, pass_filter={'COLOR'})
img_rough = bake('ROUGHNESS', 'Non-Color', 1)
img_nrm = bake('NORMAL', 'Non-Color', 4, normal_space='TANGENT')
img_ao = bake('AO', 'Non-Color', 64)


def save(img, name):
    path = os.path.join(OUT, name)
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    return path


save(img_col, "T_Molotov_BaseColor.png")
save(img_nrm, "T_Molotov_Normal.png")
# ORM: R = AO, G = roughness, B = metallic (0).
orm = bpy.data.images.new("orm", TEX, TEX, alpha=False)
orm.colorspace_settings.name = 'Non-Color'
ao_px = list(img_ao.pixels)
ro_px = list(img_rough.pixels)
px = [0.0] * (TEX * TEX * 4)
for i in range(TEX * TEX):
    k = i * 4
    px[k] = 0.35 + 0.65 * ao_px[k]   # soften the baked AO
    px[k + 1] = ro_px[k]
    px[k + 2] = 0.0
    px[k + 3] = 1.0
orm.pixels = px
save(orm, "T_Molotov_ORM.png")

# Scale up only after baking: the shaders read real-size object coordinates.
mol.scale = (PROP_SCALE,) * 3
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

# ---------------------------------------------------------------- final single-material preview setup (textured)
final = new_mat("M_Molotov")
nt = final.node_tree
bsdf = nt.nodes["Principled BSDF"]
tcol = nt.nodes.new("ShaderNodeTexImage"); tcol.image = img_col
tn = nt.nodes.new("ShaderNodeTexImage"); tn.image = img_nrm
nm = nt.nodes.new("ShaderNodeNormalMap")
trough = nt.nodes.new("ShaderNodeTexImage"); trough.image = img_rough
nt.links.new(tcol.outputs[0], bsdf.inputs["Base Color"])
nt.links.new(trough.outputs[0], bsdf.inputs["Roughness"])
nt.links.new(tn.outputs[0], nm.inputs["Color"])
nt.links.new(nm.outputs[0], bsdf.inputs["Normal"])
mol.data.materials.clear()
mol.data.materials.append(final)

# ---------------------------------------------------------------- export
bpy.ops.object.select_all(action='DESELECT')
mol.select_set(True)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SM_Molotov.fbx"), use_selection=True,
                         apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='OFF', add_leaf_bones=False,
                         bake_space_transform=False, axis_forward='-Y', axis_up='Z', path_mode='STRIP')
tip = max((mol.matrix_world @ v.co for v in mol.data.vertices), key=lambda p: p.z)
print("MOLOTOV_STATS tris=%d height=%.3f tip=(%.4f,%.4f,%.4f)" % (
    sum(len(p.vertices) - 2 for p in mol.data.polygons), max(v.co.z for v in mol.data.vertices), tip.x, tip.y, tip.z))

# ---------------------------------------------------------------- preview renders + icon render
scene.render.engine = 'CYCLES'
scene.cycles.samples = 64
scene.render.film_transparent = True
scene.render.resolution_x = 900
scene.render.resolution_y = 900
world = bpy.data.worlds.new("W")
scene.world = world
try:
    world.use_nodes = True
except Exception:
    pass
world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.37, 0.4, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.7

key = link(bpy.data.objects.new("Key", bpy.data.lights.new("Key", 'AREA')))
key.data.energy = 120; key.data.size = 0.6; key.data.color = (1.0, 0.93, 0.85)
key.location = (0.6, -0.7, 0.9); key.rotation_euler = (math.radians(55), 0, math.radians(40))
rim = link(bpy.data.objects.new("Rim", bpy.data.lights.new("Rim", 'AREA')))
rim.data.energy = 90; rim.data.size = 0.4; rim.data.color = (0.8, 0.88, 1.0)
rim.location = (-0.6, 0.6, 0.8); rim.rotation_euler = (math.radians(-55), 0, math.radians(-135))

cam = link(bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")))
scene.camera = cam
cam.data.lens = 70
center = Vector((0, 0, 0.31))


def shoot(name, az, el, dist, target=center):
    d = Vector((math.cos(math.radians(el)) * math.sin(math.radians(az)),
                -math.cos(math.radians(el)) * math.cos(math.radians(az)),
                math.sin(math.radians(el))))
    cam.location = target + d * dist
    cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = os.path.join(OUT, name)
    bpy.ops.render.render(write_still=True)


key.data.energy = 60
shoot("preview_front.png", 20, 10, 1.9)
shoot("preview_back.png", 200, 15, 1.9)
shoot("preview_top.png", 30, 55, 1.6, Vector((0, 0, 0.5)))

# Icon: tilted bottle with a burning rag (icon-only flame mesh, emissive).
mol.rotation_euler = (0, math.radians(-38), 0)
bm = bmesh.new()
bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=0.05, radius2=0.0, depth=0.22)
for v in bm.verts:
    t = (v.co.z + 0.11) / 0.22
    v.co.x += 0.03 * math.sin(t * 5.0) * t
    v.co.y += 0.02 * math.cos(t * 4.0) * t
flame = make_obj("IconFlame", bm)
fm = new_mat("MAT_IconFlame")
fnt = fm.node_tree
for n in list(fnt.nodes):
    if n.type == 'BSDF_PRINCIPLED':
        fnt.nodes.remove(n)
em = fnt.nodes.new("ShaderNodeEmission")
tcf = fnt.nodes.new("ShaderNodeTexCoord")
sepf = fnt.nodes.new("ShaderNodeSeparateXYZ")
fnt.links.new(tcf.outputs["Generated"], sepf.inputs[0])
ramp = fnt.nodes.new("ShaderNodeValToRGB")
ramp.color_ramp.elements[0].color = (1.0, 0.85, 0.35, 1)
ramp.color_ramp.elements[1].color = (0.9, 0.18, 0.02, 1)
fnt.links.new(sepf.outputs["Z"], ramp.inputs[0])
fnt.links.new(ramp.outputs[0], em.inputs["Color"])
em.inputs["Strength"].default_value = 18.0
tr = fnt.nodes.new("ShaderNodeBsdfTransparent")
mixs = fnt.nodes.new("ShaderNodeMixShader")
fz = math_node(fnt, 'POWER', sepf.outputs["Z"], 0.8)
fnt.links.new(fz, mixs.inputs[0])
fnt.links.new(em.outputs[0], mixs.inputs[1])
fnt.links.new(tr.outputs[0], mixs.inputs[2])
fnt.links.new(mixs.outputs[0], fnt.nodes["Material Output"].inputs[0])
flame.data.materials.append(fm)
bpy.context.view_layer.update()
tip_world = mol.matrix_world @ Vector((0, 0, 0.272 * PROP_SCALE))
flame.location = tip_world + Vector((0.03, 0, 0.10))
flame.rotation_euler = (0, math.radians(-15), 0)
corners = [mol.matrix_world @ Vector(c) for c in mol.bound_box] + [flame.location + Vector((0, 0, 0.11))]
icon_center = sum(corners, Vector()) / len(corners)
scene.render.resolution_x = 512
scene.render.resolution_y = 512
flame.hide_render = True  # the icon flame is painted in 2D (make_icons.py)
cam.data.lens = 50
key.data.energy = 60
shoot("icon_render.png", 15, 18, 1.55, icon_center)
flame.hide_render = True
mol.rotation_euler = (0, 0, 0)

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "molotov.blend"))
print("MOLOTOV_DONE", OUT)
