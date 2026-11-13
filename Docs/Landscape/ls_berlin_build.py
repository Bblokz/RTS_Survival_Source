"""Builds the Ger_Berlin landscape material: M_RTS_Landscape_Berlin, MI_RTS_Landscape_Berlin and the six layer infos
(Docs/Landscape/LandscapeBerlin.md).

Pipeline, left to right:
  six layers (two-scale albedo + normal, tint, roughness, normal strength, luminance height)
  -> LandscapeLayerBlend (height blend, weight-blended layer infos)
  -> MF_RTS_ColorVariation_Landscape (the RVT output reads here: RVT_Environment + RVT_Height)
  -> MF_RTS_LS_TankTrails [LS_TankTrails] (SoftGround = saturate(Rubble*0.6 + Ash + Scrub + Mud))
  -> BreakMaterialAttributes -> material outputs.
The shared functions are only called, never rebuilt (a rebuild gives their inputs new ids and unhooks every caller).
Re-runnable: the master is rebuilt in place (every expression deleted one by one, then recreated); the instance and
the layer infos are created only when missing, so their overrides survive. Missing textures fall back to the engine
defaults and are logged; run again after importing the real ones.
Usage (UECP python or -run=pythonscript):
  sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape"); import ls_berlin_build; ls_berlin_build.run()
"""
import importlib
import os

import unreal

import ls_graph as G

importlib.reload(G)
from ls_graph import N, C, S, V, mul, lerp, mask

BERLIN_DIR = "/Game/RTS_Survival/Materials/ground/Landscape/Berlin"
TEXTURE_DIR = BERLIN_DIR + "/Textures"
DATA_DIR = BERLIN_DIR + "/DATA"
MAT_NAME = "M_RTS_Landscape_Berlin"
MI_NAME = "MI_RTS_Landscape_Berlin"
MAT_PATH = BERLIN_DIR + "/" + MAT_NAME
MI_PATH = BERLIN_DIR + "/" + MI_NAME
COLOR_VARIATION_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/MF_RTS_ColorVariation_Landscape"
TANK_TRAILS_PATH = G.FUNCTION_DIR + "/MF_RTS_LS_TankTrails"
NOISE_PATH = "/Game/RTS_Survival/Textures/Noise/T_RTSNoise_MacroVariation"
FALLBACK_DIFFUSE = "/Engine/EngineMaterials/DefaultDiffuse"
FALLBACK_NORMAL = "/Engine/EngineMaterials/DefaultNormal"
DESC = "LSBerlin"
GLOBAL_GROUP = "Berlin Global"
TRAILS_GROUP = "Tank Trails"

# name, preview weight (all > 0: off a landscape a zero preview weight compiles the layer out), height offset,
# height contrast, luma mid, debug colour. Hard layers sit higher, so they poke through the soft ones at a transition.
# Luma mid = the median linear luminance of the generated albedo (Saved/Berlin/textures, 2026-11-12); contrast is about
# 0.25 / its standard deviation, so one deviation moves the height by ~0.25 (more on the crisp hard layers).
LAYERS = [
    ("BerlinRubble", 1.0, 0.05, 6.0, 0.156, (0.55, 0.45, 0.35)),
    ("BerlinAsh", 0.2, -0.10, 3.0, 0.265, (0.75, 0.75, 0.75)),
    ("BerlinCobble", 0.2, 0.15, 8.0, 0.160, (0.35, 0.35, 0.45)),
    ("BerlinGlass", 0.2, 0.10, 6.0, 0.030, (0.20, 0.70, 0.60)),
    ("BerlinScrub", 0.2, 0.00, 4.0, 0.065, (0.60, 0.55, 0.20)),
    ("BerlinMud", 0.2, -0.15, 8.0, 0.045, (0.30, 0.20, 0.10)),
]
LAYER_NAMES = [layer[0] for layer in LAYERS]
# Tank trails dent these (weight factor); cobble and glass keep no ruts.
SOFT_LAYERS = [("BerlinRubble", 0.6), ("BerlinAsh", 1.0), ("BerlinScrub", 1.0), ("BerlinMud", 1.0)]
TILE_SIZE_CM = 500.0
LUMA = unreal.LinearColor(0.299, 0.587, 0.114, 0.0)
# Root pins the other masters feed from their main BreakMaterialAttributes.
ROOT_PINS = [("BaseColor", unreal.MaterialProperty.MP_BASE_COLOR),
             ("Metallic", unreal.MaterialProperty.MP_METALLIC),
             ("Specular", unreal.MaterialProperty.MP_SPECULAR),
             ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS),
             ("Normal", unreal.MaterialProperty.MP_NORMAL),
             ("EmissiveColor", unreal.MaterialProperty.MP_EMISSIVE_COLOR),
             ("AmbientOcclusion", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)]

ROW = 900  # graph rows per layer
fallbacks = []


def log(text):
    unreal.log("[ls_berlin_build] " + text)


# ---------------------------------------------------------------------------------------------------------------- assets

def asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def disk_path(asset_path):
    return G.CONTENT_DIR + asset_path[len("/Game/"):] + ".uasset"


def save(asset):
    """Saves and checks the file landed; an editor holding the package makes the save fail ("Error moving file")."""
    path = asset.get_path_name().split(".")[0]
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        G.failures.append("save failed: " + path)
    elif not os.path.exists(disk_path(path)):
        G.failures.append("not on disk: " + disk_path(path))


def texture(layer, suffix):
    """T_Berlin_<LayerName>_<suffix> (T_Berlin_BerlinRubble_D, as berlin_textures_gen.py names them), else the short
    form T_Berlin_Rubble_D, else the engine default."""
    candidates = ["%s/T_Berlin_%s_%s" % (TEXTURE_DIR, name, suffix) for name in (layer, layer[len("Berlin"):])]
    for path in candidates:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return unreal.load_asset(path)
    path = candidates[0]
    fallback = FALLBACK_NORMAL if suffix == "N" else FALLBACK_DIFFUSE
    fallbacks.append(path)
    log("missing %s, using %s" % (path, fallback))
    return unreal.load_asset(fallback)


def sampler_type(tex):
    """The sampler type the texture's own settings need: a mismatch is a compile error."""
    compression = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    T, ST = unreal.TextureCompressionSettings, unreal.MaterialSamplerType
    if compression == T.TC_NORMALMAP:
        return ST.SAMPLERTYPE_NORMAL
    if compression == T.TC_MASKS:
        return ST.SAMPLERTYPE_MASKS
    if compression == T.TC_ALPHA:
        return ST.SAMPLERTYPE_ALPHA
    if compression in (T.TC_GRAYSCALE, T.TC_DISPLACEMENTMAP, T.TC_DISTANCE_FIELD_FONT):
        return ST.SAMPLERTYPE_GRAYSCALE if srgb else ST.SAMPLERTYPE_LINEAR_GRAYSCALE
    return ST.SAMPLERTYPE_COLOR if srgb else ST.SAMPLERTYPE_LINEAR_COLOR


def sample(tex, uv, x, y):
    """TextureSample on the shared wrap sampler: costs no sampler slot, however many textures the layers use."""
    e = N("TextureSample", x, y, texture=tex, sampler_type=sampler_type(tex),
          sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    C(uv, "", e, "UVs")
    return e


def material_asset():
    """The master, created on first use; on a rebuild every expression is deleted one by one (the asset itself is kept,
    so the instance's parent link and overrides stay)."""
    if not unreal.EditorAssetLibrary.does_asset_exist(MAT_PATH):
        asset_tools().create_asset(MAT_NAME, BERLIN_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material = unreal.load_asset(MAT_PATH)
    for _ in range(4):
        live = G.live_expressions(material)
        if not live:
            break
        for e in live:
            G.mel.delete_material_expression(material, e)
    left = G.live_expressions(material)
    if left:
        raise RuntimeError("%d expressions survived the clear of %s" % (len(left), MAT_PATH))
    # Same settings as M_RTS_Landscape_DryCastle / DesertPaintable.
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("tangent_space_normal", True)
    material.set_editor_property("use_material_attributes", False)
    material.set_editor_property("used_with_nanite", True)
    return material


def instance_asset(material):
    """MI_RTS_Landscape_Berlin, created on first use with trails on; an existing one keeps all its overrides."""
    if unreal.EditorAssetLibrary.does_asset_exist(MI_PATH):
        mi = unreal.load_asset(MI_PATH)
        if mi.get_editor_property("parent") != material:
            mi.set_editor_property("parent", material)
        return mi, False
    asset_tools().create_asset(MI_NAME, BERLIN_DIR, unreal.MaterialInstanceConstant,
                               unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(MI_PATH)
    mi.set_editor_property("parent", material)
    # TankTrailStrength is MF_RTS_LS_TankTrails' own parameter; above 0 it tells UTankTrailSubsystem to draw here.
    # (Both setters return False on a fresh instance even when they apply, so read the values back instead.)
    G.mel.set_material_instance_static_switch_parameter_value(mi, "LS_TankTrails", True)
    G.mel.set_material_instance_scalar_parameter_value(mi, "TankTrailStrength", 1.0)
    G.mel.update_material_instance(mi)
    if not G.mel.get_material_instance_static_switch_parameter_value(mi, "LS_TankTrails"):
        G.failures.append("MI: LS_TankTrails not on")
    if G.mel.get_material_instance_scalar_parameter_value(mi, "TankTrailStrength") != 1.0:
        G.failures.append("MI: TankTrailStrength not 1")
    return mi, True


def layer_infos():
    """<Layer>_LayerInfo, weight-blended, created only when missing. The DryCastle and DesertPaintable infos carry no
    physical material, so neither do these."""
    created = []
    for name, *_, color in LAYERS:
        path = "%s/%s_LayerInfo" % (DATA_DIR, name)
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            info = unreal.load_asset(path)
        else:
            asset_tools().create_asset(name + "_LayerInfo", DATA_DIR, unreal.LandscapeLayerInfoObject, None)
            info = unreal.load_asset(path)
            if info is None:
                G.failures.append("layer info not created: " + path)
                continue
            # LayerName is VisibleAnywhere (read-only to set_editor_property); the SET command imports it as text
            # on this one object, the same way the landscape editor's "create layer info" fills it in.
            unreal.SystemLibrary.execute_console_command(
                None, "set %s LayerName %s" % (info.get_path_name(), name))
            info.set_editor_property("no_weight_blend", False)
            info.set_editor_property("layer_usage_debug_color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
            if str(info.get_editor_property("layer_name")) == name:
                save(info)
                created.append(name)
        if str(info.get_editor_property("layer_name")) != name:
            # Never leave a misnamed info on disk: the landscape would paint a layer the material does not know.
            G.failures.append("layer info %s has layer name %s (not saved)" % (path, info.get_editor_property("layer_name")))
    return created


# ----------------------------------------------------------------------------------------------------------------- graph

def append3(a, b, c, x, y):
    ab = N("AppendVector", x, y)
    C(a, "", ab, "A")
    C(b, "", ab, "B")
    abc = N("AppendVector", x + 140, y)
    C(ab, "", abc, "A")
    C(c, "", abc, "B")
    return abc


def shared_inputs(x, y):
    """World-space UV in metres, the macro-noise sample and the two values every layer reads from it."""
    G.comment("Shared: world XY in metres (the layers scale it by their tile size), one macro-noise sample "
              "(T_RTSNoise_MacroVariation, the texture the colour variation reads too). R picks how much of the "
              "large-scale albedo sample shows; G jitters the blend heights so layer edges break up.",
              x - 60, y - 120, 1500, 760, G.INPUT_COLOR)
    world = N("WorldPosition", x, y)
    world_xy = mask(world, "", "RG", x + 160, y)
    uv_m = G.mulc(world_xy, "", 0.01, x + 320, y)  # cm -> m

    noise_size = S("Berlin_NoiseSize", 4000.0, x, y + 160)
    inv_noise = G.op("Divide", G.const(1.0, x, y + 260), "", noise_size, "", x + 180, y + 200)
    noise_uv = mul(world_xy, "", inv_noise, "", x + 360, y + 160)
    noise = sample(unreal.load_asset(NOISE_PATH), noise_uv, x + 520, y + 140)

    blend_min = S("Berlin_MacroBlendMin", 0.2, x + 820, y + 120)
    blend_max = S("Berlin_MacroBlendMax", 0.6, x + 820, y + 220)
    blend_range = G.op("Subtract", blend_max, "", blend_min, "", x + 1000, y + 200)
    macro_alpha = G.op("Add", mul(noise, "R", blend_range, "", x + 1000, y + 100), "", blend_min, "",
                       x + 1160, y + 120)

    height_noise = S("Berlin_HeightNoise", 0.3, x + 820, y + 360)
    jitter = mul(G.addc(noise, "G", -0.5, x + 1000, y + 320), "", height_noise, "", x + 1160, y + 340)
    macro_scale = S("Berlin_MacroScale", 0.23, x + 820, y + 480)
    return uv_m, macro_alpha, jitter, macro_scale


def build_layer(index, layer, shared, x, y):
    """One layer: MakeMaterialAttributes and its blend height."""
    name, _, height_offset, height_contrast, height_luma_mid, _ = layer
    uv_m, macro_alpha, jitter, macro_scale = shared
    G.begin(G._target, DESC, name[len("Berlin"):] + " (" + name + ")")
    G.comment("%s: tile sample plus a large-scale sample of the same albedo (Berlin_MacroScale, offset per layer) "
              "mixed by the macro noise, tint, roughness from albedo alpha x %s_RoughnessScale, normal strength, and "
              "the blend height from albedo luminance." % (name, name), x - 60, y - 120, 2200, ROW - 100,
              G.LAYER_COLOR)

    tile = S(name + "_TileSize", TILE_SIZE_CM / 100.0, x, y)  # metres per tile
    inv_tile = G.op("Divide", G.const(1.0, x, y + 100), "", tile, "", x + 160, y + 40)
    uv = mul(uv_m, "", inv_tile, "", x + 320, y)
    uv_macro = G.addc(mul(uv, "", macro_scale, "", x + 320, y + 140), "", 0.37 * (index + 1), x + 460, y + 140)

    albedo_tex, normal_tex = texture(name, "D"), texture(name, "N")
    d_tile = sample(albedo_tex, uv, x + 620, y - 40)
    d_macro = sample(albedo_tex, uv_macro, x + 620, y + 240)
    n_tile = sample(normal_tex, uv, x + 620, y + 520)
    d = lerp(d_tile, "RGBA", d_macro, "RGBA", macro_alpha, "", x + 900, y)

    tint = V(name + "_Tint", unreal.LinearColor(1.0, 1.0, 1.0, 1.0), x + 900, y + 120)
    base_color = mul(mask(d, "", "RGB", x + 1060, y), "", tint, "", x + 1220, y)
    rough_scale = S(name + "_RoughnessScale", 1.0, x + 900, y + 260)
    roughness = G.sat(mul(mask(d, "", "A", x + 1060, y + 220), "", rough_scale, "", x + 1220, y + 240), "",
                      x + 1360, y + 240)

    strength = S(name + "_NormalStrength", 1.0, x + 900, y + 560)
    normal_scale = append3(strength, strength, G.const(1.0, x + 900, y + 660), x + 1060, y + 580)
    normal = mul(n_tile, "RGB", normal_scale, "", x + 1360, y + 540)

    specular = None
    if name == "BerlinGlass":
        # Vitrified slag: glossy from the texture, but never mirror-like or sparkling from the RTS camera.
        rough_min = S("BerlinGlass_RoughnessMin", 0.2, x + 1360, y + 340)
        roughness = G.op("Max", roughness, "", rough_min, "", x + 1520, y + 260)
        specular = S("BerlinGlass_Specular", 0.6, x + 1520, y + 400)
    elif name == "BerlinMud":
        # Wetness: darker and smoother; 0 = dry mud.
        wetness = S("BerlinMud_Wetness", 0.0, x + 1360, y + 340)
        darken = S("BerlinMud_WetDarken", 0.55, x + 1360, y + 440)
        wet_rough = S("BerlinMud_WetRoughness", 0.25, x + 1520, y + 440)
        dark_k = lerp(G.const(1.0, x + 1520, y + 100), "", darken, "", wetness, "", x + 1680, y + 60)
        base_color = mul(base_color, "", dark_k, "", x + 1840, y)
        roughness = lerp(roughness, "", wet_rough, "", wetness, "", x + 1680, y + 260)

    make = N("MakeMaterialAttributes", x + 2000, y)
    C(base_color, "", make, "BaseColor")
    C(roughness, "", make, "Roughness")
    C(normal, "", make, "Normal")
    if specular is not None:
        C(specular, "", make, "Specular")

    # Blend height: saturate((luma - luma mid) * contrast + 0.5 + offset + noise jitter), from the tile sample. Ground
    # albedo sits far below 0.5 linear, so each layer centres on its own texture's median luminance.
    luma = N("DotProduct", x + 900, y + 700)
    C(d_tile, "RGB", luma, "A")
    C(N("Constant3Vector", x + 760, y + 780, constant=LUMA), "", luma, "B")
    luma_mid = S(name + "_HeightLumaMid", height_luma_mid, x + 900, y + 860)
    contrast = S(name + "_HeightContrast", height_contrast, x + 900, y + 780)
    offset = S(name + "_HeightOffset", height_offset, x + 1060, y + 780)
    bias = G.addc(offset, "", 0.5, x + 1220, y + 780)  # uniform: folded into the preshader
    centred = mul(G.op("Subtract", luma, "", luma_mid, "", x + 1060, y + 700), "", contrast, "", x + 1220, y + 700)
    height = G.sat(G.op("Add", G.op("Add", centred, "", bias, "", x + 1380, y + 700), "", jitter, "",
                        x + 1540, y + 700), "", x + 1700, y + 700)
    return make, height


def layer_blend(makes_heights, x, y):
    G.begin(G._target, DESC, GLOBAL_GROUP)
    G.comment("Layer blend: height blend on weight-blended layer infos. Each layer's weight is moved by its height "
              "(hard cobble and glass sit higher, ash and mud lower), so transitions break along the textures instead "
              "of airbrushing.", x - 60, y - 120, 520, 900, G.INPUT_COLOR)
    blend = N("LandscapeLayerBlend", x, y)
    inputs = []
    for name, preview, *_ in LAYERS:
        layer = unreal.LayerBlendInput()
        layer.set_editor_property("layer_name", name)
        layer.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_HEIGHT_BLEND)
        layer.set_editor_property("preview_weight", preview)
        inputs.append(layer)
    blend.set_editor_property("layers", inputs)
    for (name, *_), (make, height) in zip(LAYERS, makes_heights):
        C(make, "", blend, "Layer " + name)
        C(height, "", blend, "Height " + name)
    return blend


def soft_ground(x, y):
    """saturate(Rubble*0.6 + Ash + Scrub + Mud): the tank trails dent only these."""
    total = None
    for index, (name, factor) in enumerate(SOFT_LAYERS):
        weight = N("LandscapeLayerSample", x, y + index * 110, parameter_name=name, preview_weight=0.0)
        if factor != 1.0:
            weight = G.mulc(weight, "", factor, x + 180, y + index * 110)
        total = weight if total is None else G.op("Add", total, "", weight, "", x + 320 + index * 40,
                                                  y + index * 80)
    return G.sat(total, "", x + 560, y + 160)


def build(material):
    G.begin(material, DESC, GLOBAL_GROUP)
    LX, LY = -5200, -2400
    shared = shared_inputs(LX, LY - 1000)
    makes_heights = [build_layer(i, layer, shared, LX, LY + i * ROW) for i, layer in enumerate(LAYERS)]
    G.begin(material, DESC, GLOBAL_GROUP)
    blend = layer_blend(makes_heights, -2600, 0)

    G.comment("Colour variation (MF_RTS_ColorVariation_Landscape, as in DryCastle), then the runtime virtual texture "
              "output (RVT_Environment: base colour, specular, roughness, world normal; RVT_Height: world Z). The RVT "
              "caches its pages, so it reads the ground here: nothing after this point may change it.",
              -2000, -760, 1300, 1160, G.OUTPUT_COLOR)
    color_var = G.function_call(unreal.load_asset(COLOR_VARIATION_PATH), -1900, 0)
    C(G.const(4.0, -2000, 160), "", color_var, "InNoiseTillling")
    C(G.const(0.5, -2000, 240), "", color_var, "InNoiseStrength")
    C(blend, "", color_var, "InputMaterial")
    rvt_break = N("BreakMaterialAttributes", -1600, -600)
    C(color_var, "Result", rvt_break, "Attr")
    to_world = N("Transform", -1340, -460,
                 transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    C(rvt_break, "Normal", to_world, "")
    world_z = mask(N("WorldPosition", -1340, -340), "", "B", -1180, -340)
    rvt = N("RuntimeVirtualTextureOutput", -1000, -600)
    for pin in ["BaseColor", "Specular", "Roughness"]:
        C(rvt_break, pin, rvt, pin)
    C(to_world, "", rvt, "Normal")
    C(world_z, "", rvt, "WorldHeight")

    G.begin(material, DESC, TRAILS_GROUP)
    G.comment("Tank trails (LS_TankTrails, off in the master, on in MI_RTS_Landscape_Berlin): ruts from RT_TankTrails "
              "on soft ground only (rubble at 60 %, ash, scrub, mud). TankTrailStrength > 0 tells UTankTrailSubsystem "
              "to draw on this map. Main pass only.", -640, -200, 1000, 900)
    trails = unreal.load_asset(TANK_TRAILS_PATH)
    if trails is None:
        raise RuntimeError("missing " + TANK_TRAILS_PATH + " (never rebuild it from here)")
    trails_call = G.function_call(trails, -200, 0)
    C(color_var, "Result", trails_call, "Attributes")
    C(soft_ground(-600, 300), "", trails_call, "SoftGround")
    switch = N("StaticSwitchParameter", 160, 0, parameter_name="LS_TankTrails", default_value=False,
               group=TRAILS_GROUP)
    C(trails_call, "Attributes", switch, "True")
    C(color_var, "Result", switch, "False")

    G.begin(material, DESC, GLOBAL_GROUP)
    G.comment("Material outputs", 360, -200, 400, 700, G.OUTPUT_COLOR)
    out_break = N("BreakMaterialAttributes", 420, 0)
    C(switch, "", out_break, "Attr")
    for pin, prop in ROOT_PINS:
        if not G.mel.connect_material_property(out_break, pin, prop):
            G.failures.append("root pin " + pin)
    library = getattr(unreal, "RTSMaterialGraphLibrary", None)
    if library:
        library.set_material_output_position(material, 800, 0)


# ----------------------------------------------------------------------------------------------------------------- stats

def statistics(material, mi):
    """(pixel instructions, samplers, estimated pixel texture samples) with LS_TankTrails off and on. Off is the
    master's own shader map (switch default off, the same permutation an instance with it off uses); on is the saved
    instance. Toggling the switch on an instance, or on transient children of it, returned unreliable numbers, so
    nothing is toggled. 0 instructions / samplers -1 = a broken permutation."""
    result = {}
    mi_on = bool(G.mel.get_material_instance_static_switch_parameter_value(mi, "LS_TankTrails"))
    for label, asset in [("trails off (master)", material),
                         ("trails %s (%s)" % ("on" if mi_on else "off", MI_NAME), mi)]:
        st = G.mel.get_statistics(asset)
        result[label] = (st.num_pixel_shader_instructions, st.num_samplers, st.num_pixel_texture_samples)
        if st.num_pixel_shader_instructions <= 0 or st.num_samplers < 0:
            G.failures.append("broken permutation: " + label)
    return result


def run(stats=True):
    G.failures.clear()
    del fallbacks[:]
    created_infos = layer_infos()
    material = material_asset()
    build(material)
    G.mel.recompile_material(material)
    save(material)
    mi, mi_created = instance_asset(material)
    report = {"layer infos created": created_infos, "instance created": mi_created, "fallback textures": list(fallbacks)}
    if stats:
        report["stats (all six layers, worst case: ps instructions, samplers, ps texture samples)"] = statistics(material, mi)
    save(mi)
    report["failures"] = list(G.failures)
    log(str(report))
    return report
