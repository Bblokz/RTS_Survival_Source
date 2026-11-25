"""In-editor steps for the Ger_FrozenLake north backdrop (dry lakebed + Svarog crater rim). Data: fl_backdrop_gen.py.

Run through the throwaway editor (fl_editor_server.py / fl_cmd.py), with fl_map_build loaded and the map open:

    import fl_backdrop_build as fbb
    fbb.SAVE = False                 # concept round: nothing is written to disk
    fbb.build_material()
    fbb.place()                      # the picked concept: heights into the landscape margin, mask, mesh, actor

Backdrop rules (decision 21): no collision, no navigation, no shadows, hidden in scene captures (minimap), one cheap
lit material on world XY. The landscape margin past the play edge's blockers is pushed under the mesh; nothing inside
the blockers is touched.
"""
import json
import os

import unreal

import fl_map_build as fmb

SAVE = True
DATA = "D:/UE5Projects/RTS_Survival/Saved/FrozenLake/backdrop/"
DIR = fmb.FL_ROOT + "Backdrop"
MAT = "M_FL_Backdrop"
LABEL = fmb.PREFIX + "Backdrop"
FOLDER = fmb.FOLDER + "Backdrop"

T_ARID = "/Game/Megascans/Surfaces/Arid_Soil_Ground_wihhfexbw/T_Arid_Soil_Ground_wihhfexbw_2K_"
T_BURNT = "/Game/Megascans/Surfaces/Burnt_Forest_Floor_uhwpehcdy/T_Burnt_Forest_Floor_uhwpehcdy_4K_"
T_SULPHUR = "/Game/Megascans/Surfaces/Sulphur_Rock_vmijee1g/T_Sulphur_Rock_vmijee1g_2K_"
NOISE = "/Game/RTS_Survival/Textures/Noise/T_RTSNoise_MacroVariation"

mel = unreal.MaterialEditingLibrary
ST = unreal.MaterialSamplerType


def _save(asset):
    if SAVE:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)


def _info(concept):
    with open(DATA + concept + "/backdrop.json", "r") as handle:
        return json.load(handle)


def build_material(use_normal=True):
    """Lit, opaque, world-XY material. Mask texture (param BackdropMask): R scorch, G glass, B glow, A wet."""
    path = DIR + "/" + MAT
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mat = unreal.load_asset(path)
        for _ in range(12):
            if mel.get_num_material_expressions(mat) == 0:
                break
            mel.delete_all_material_expressions(mat)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MAT, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("used_with_nanite", True)        # the OBJ import makes a Nanite mesh
    pos = [-3000, -700]

    def N(cls, **props):
        e = mel.create_material_expression(mat, getattr(unreal, "MaterialExpression" + cls), pos[0], pos[1])
        pos[1] += 90
        if pos[1] > 1100:
            pos[0], pos[1] = pos[0] + 260, -700
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def C(a, out, b, inp):
        if not mel.connect_material_expressions(a, out, b, inp):
            raise RuntimeError("connect failed %s.%s -> %s.%s" % (a.get_name(), out, b.get_name(), inp))

    def S(name, value, group="Backdrop"):
        return N("ScalarParameter", parameter_name=name, default_value=value, group=group)

    def V(name, rgb, group="Backdrop"):
        return N("VectorParameter", parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0),
                 group=group)

    def op(cls, a, ao, b, bo):
        e = N(cls)
        C(a, ao, e, "A")
        C(b, bo, e, "B")
        return e

    def mulc(a, out, k):
        e = N("Multiply", const_b=k)
        C(a, out, e, "A")
        return e

    def addc(a, out, k):
        e = N("Add", const_b=k)
        C(a, out, e, "A")
        return e

    def lerp(a, ao, b, bo, al, alo):
        e = N("LinearInterpolate")
        C(a, ao, e, "A")
        C(b, bo, e, "B")
        C(al, alo, e, "Alpha")
        return e

    def one(a, out, cls):
        e = N(cls)
        C(a, out, e, "")
        return e

    def sample(tex, uv, kind, clamp=False):
        src = unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS if clamp else \
            unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS
        e = N("TextureSample", texture=unreal.load_asset(tex), sampler_type=kind, sampler_source=src)
        C(uv, "", e, "UVs")
        return e

    world = N("WorldPosition")
    xy = N("ComponentMask", r=True, g=True, b=False, a=False)
    C(world, "", xy, "")
    wx = N("ComponentMask", r=True, g=False, b=False, a=False)
    C(world, "", wx, "")
    wy = N("ComponentMask", r=False, g=True, b=False, a=False)
    C(world, "", wy, "")

    # mask UV: U = (y - MeshMinY) / MeshSizeY (west left), V = (MeshMaxX - x) / MeshSizeX (north up)
    mu = op("Divide", op("Subtract", wy, "", S("MeshMinY", -72000.0, "Extent"), ""), "", S("MeshSizeY", 144000.0, "Extent"), "")
    mv = op("Divide", op("Subtract", S("MeshMaxX", 112000.0, "Extent"), "", wx, ""), "", S("MeshSizeX", 82200.0, "Extent"), "")
    muv = N("AppendVector")
    C(mu, "", muv, "A")
    C(mv, "", muv, "B")
    mask = N("TextureSampleParameter2D", parameter_name="BackdropMask", sampler_type=ST.SAMPLERTYPE_LINEAR_COLOR,
             sampler_source=unreal.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS,
             texture=unreal.load_asset("/Engine/EngineResources/Black"))
    C(muv, "", mask, "UVs")

    # lakebed: cracked arid soil, two scales mixed by macro noise, tinted pale
    uv_bed = op("Divide", xy, "", S("BedTile", 1800.0), "")
    uv_bed2 = addc(mulc(uv_bed, "", 0.19), "", 0.37)
    noise = sample(NOISE, mulc(xy, "", 1.0 / 6000.0), ST.SAMPLERTYPE_COLOR)
    bed_d = lerp(sample(T_ARID + "D", uv_bed, ST.SAMPLERTYPE_COLOR), "RGB",
                 sample(T_ARID + "D", uv_bed2, ST.SAMPLERTYPE_COLOR), "RGB", addc(mulc(noise, "R", 0.5), "", 0.2), "")
    # the Megascans arid soil is a dark red clay (about 0.03 linear luminance): keep its cracks, lose its colour,
    # and lift it to a pale grey silt
    bed_grey = N("Desaturation")
    C(bed_d, "", bed_grey, "")
    C(N("Constant", r=1.0), "", bed_grey, "Fraction")
    bed = op("Multiply", mulc(bed_grey, "", 1.0), "", op("Multiply", V("BedTint", (0.95, 0.92, 0.85)), "",
                                                        S("BedBrightness", 20.0), ""), "")
    bed_n = sample(T_ARID + "N", uv_bed, ST.SAMPLERTYPE_NORMAL)
    # scorch: burnt forest floor, darkened
    uv_sc = op("Divide", xy, "", S("ScorchTile", 1200.0), "")
    sc = op("Multiply", sample(T_BURNT + "D", uv_sc, ST.SAMPLERTYPE_COLOR), "RGB", V("ScorchTint", (0.8, 0.74, 0.7)), "")
    # glass: near-black, glossy, the sulphur rock's facets for its normal
    uv_gl = op("Divide", xy, "", S("GlassTile", 1500.0), "")
    gl_n = sample(T_SULPHUR + "N", uv_gl, ST.SAMPLERTYPE_NORMAL)
    gl_col = V("GlassColor", (0.018, 0.026, 0.03))
    wet_col = V("WetColor", (0.012, 0.014, 0.018))

    base = lerp(bed, "", sc, "", mask, "R")
    base = lerp(base, "", gl_col, "", mask, "G")
    base = lerp(base, "", wet_col, "", mask, "A")
    rough = lerp(S("BedRoughness", 0.9), "", S("ScorchRoughness", 0.95), "", mask, "R")
    rough = lerp(rough, "", S("GlassRoughness", 0.12), "", mask, "G")
    rough = lerp(rough, "", S("WetRoughness", 0.06), "", mask, "A")
    spec = lerp(N("Constant", r=0.4), "", S("GlassSpecular", 0.9), "", mask, "G")
    normal = lerp(bed_n, "RGB", gl_n, "RGB", mask, "G")
    flat = N("Constant3Vector", constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    normal = lerp(normal, "", flat, "", mask, "A")

    # far fade: darker with distance outside the landscape square, so the ring ends in the night, not in a line
    outside = N("Max", const_b=0.0)
    C(op("Subtract", one(xy, "", "Abs"), "", S("RimHalfSize", 37800.0, "Extent"), ""), "", outside, "A")
    dist = one(outside, "", "Length")
    fade_k = one(op("Divide", op("Subtract", dist, "", S("FadeStart", 40000.0), ""), "", S("FadeLength", 35000.0), ""), "", "Saturate")
    fade = lerp(N("Constant", r=1.0), "", S("FarBrightness", 0.08), "", fade_k, "")
    color = op("Multiply", base, "", fade, "")
    glow = op("Multiply", op("Multiply", V("GlowColor", (1.0, 0.42, 0.06)), "", S("GlowStrength", 6.0), ""), "", mask, "B")
    glow = op("Multiply", glow, "", fade, "")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    if use_normal:
        mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    _save(mat)
    st = mel.get_statistics(mat)
    print("material %s: %d expressions, %d pixel instructions, %d samples" % (
        path, mel.get_num_material_expressions(mat), st.num_pixel_shader_instructions, st.num_pixel_texture_samples))
    return mat


def _mi(name, mask_tex, info):
    path = DIR + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(DIR + "/" + MAT))
    mel.clear_all_material_instance_parameters(mi)
    mel.set_material_instance_texture_parameter_value(mi, "BackdropMask", mask_tex)
    mx, my = info["mesh_x"], info["mesh_y"]
    for k, v in (("MeshMinY", my[0]), ("MeshSizeY", my[1] - my[0]), ("MeshMaxX", mx[1]), ("MeshSizeX", mx[1] - mx[0])):
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.set_material_instance_scalar_parameter_value(mi, "GlowStrength", info.get("glow_strength", 0.0))
    mel.set_material_instance_scalar_parameter_value(mi, "RimHalfSize", info.get("rim_half", 37800.0))
    mel.update_material_instance(mi)
    _save(mi)
    return mi


PICKED = "C"          # owner pick 2026-11-24: "Radixite scar"


def place(concept=PICKED, remove_trees=True, capture_visible=False):
    """Landscape margin heights, mask, mesh and the actor (replaces any earlier backdrop). The picked concept gets
    the plain asset names; a concept round (SAVE False) gets suffixed ones. capture_visible: let scene captures see
    it (checks only; the minimap must not)."""
    fmb.require_map()
    info = _info(concept)
    folder = DATA + concept + "/"
    sfx = "" if concept == PICKED else "_" + concept
    landscape = fmb.by_label()[fmb.LANDSCAPE_LABEL]
    ok = unreal.RTSShortsLandscapeLibrary.import_stage_heights(landscape, folder + "FL_height_backdrop.raw")
    tex = fmb._import(folder + "T_FL_BackdropMask.png", DIR, "T_FL_BackdropMask" + sfx, texture=True)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    _save(tex)
    mesh = fmb._import(folder + "SM_FL_Backdrop.obj", DIR, "SM_FL_Backdrop" + sfx)
    # no distance field: the backdrop lies outside the play area and must not shade anything
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    build = sms.get_lod_build_settings(mesh, 0)
    build.set_editor_property("distance_field_resolution_scale", 0.0)
    sms.set_lod_build_settings(mesh, 0, build)
    mesh.set_material(0, _mi("MI_FL_Backdrop" + sfx, tex, info))
    _save(mesh)
    fmb.clear_prefix(LABEL)
    actor = fmb.actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_collision_profile_name("NoCollision")
    comp.set_editor_property("can_ever_affect_navigation", False)
    comp.set_editor_property("cast_shadow", False)
    comp.set_editor_property("affect_distance_field_lighting", False)
    comp.set_editor_property("hidden_in_scene_capture", not capture_visible)   # keep it off the minimap
    actor.set_actor_label(LABEL)
    actor.set_folder_path(FOLDER)
    box = mesh.get_bounding_box()
    removed = 0
    if remove_trees:
        # trees on ground the margin lost (past the old shore line) would float or stand in the bed
        for a in fmb.all_actors():
            if a.get_actor_label().startswith(fmb.TREE_PREFIX) and a.get_actor_location().x > fmb.TREE_MAX_X:
                fmb.actors_sub().destroy_actor(a)
                removed += 1
    print("backdrop %s: heights %s, mesh bounds x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f, trees removed %d" % (
        concept, ok, box.min.x, box.max.x, box.min.y, box.max.y, box.min.z, box.max.z, removed))
    return actor
