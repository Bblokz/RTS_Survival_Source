"""Street litter for Ger_Berlin: paper, ash flakes and dust wisps drifting along the lanes (headless, idempotent).

    sh Source/Tools/BerlinMap/ambient/run_headless.sh D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ambient/ambient_litter_build.py litter

Builds, under /Game/RTS_Survival/Blueprints/Environment/Berlin/Ambient/Litter:
  T_BerlinAmbient_Litter      4x4 atlas from ambient_textures_gen.py (12 tumbling paper frames, 4 ash flakes)
  M_BerlinAmbient_Litter      masked, lit sprite material (base pass, no blending, no overdraw cost)
  M_BerlinAmbient_Wisp        lit translucent dust wisp on the project's T_AmbientDust_Puffs
  NET_BerlinAmbient           Niagara effect type: instances further than CULL_DISTANCE from the camera are put to sleep
  NS_BerlinStreetLitter       ONE system of three stateless ("lightweight") emitters: Paper, Ash, Wisp. Stateless emitters
                              keep no particle state: positions are a closed-form function of age evaluated on the GPU.
The system is made by the project's own RTS.ImpactVfx.BuildFromRecipe (stateless recipe key); the recipe is written to
Saved/Berlin/ambient/street_litter_recipe.json. One system asset; ambient_place.py puts one component per lane stretch,
rotated so local +X runs down the lane with the wind.

A stretch is a box STRETCH_LENGTH x lane width; particles start anywhere in it and drift down local +X.
"""
import json
import os

import unreal

FOLDER = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Ambient/Litter"
TEXTURE_DIR = "D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/textures"
RECIPE_FILE = "D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/street_litter_recipe.json"
FOUNTAIN = "/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight"
PUFFS = "/Game/RTS_Survival/Blueprints/Environment/AmbientFx/T_AmbientDust_Puffs"
SYSTEM = FOLDER + "/NS_BerlinStreetLitter"
EFFECT_TYPE = FOLDER + "/NET_BerlinAmbient"

STRETCH_LENGTH = 3000.0     # cm, local X; ambient_place.py spaces the components to match
STRETCH_WIDTH = 700.0       # cm, local Y (component scale Y widens it for the 9 m lanes)
CULL_DISTANCE = 11000.0     # cm from the camera; the RTS camera sits 20-40 m up and sees about 65 x 36 m of ground

MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(text):
    unreal.log("AMB_LITTER " + str(text))


def save(asset):
    ok = ASSETS.save_loaded_asset(asset, False)
    log("saved %s %s" % (asset.get_path_name().split(".")[0], ok))
    return ok


# ---- texture -----------------------------------------------------------------------------------------------------------
def import_texture(name, srgb=True):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(TEXTURE_DIR, name + ".png")
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + name)
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    save(texture)
    return texture


# ---- materials ---------------------------------------------------------------------------------------------------------
def fresh_material(name):
    path = FOLDER + "/" + name
    if ASSETS.does_asset_exist(path):
        material = unreal.load_asset(path)
        for _ in range(20):                         # one delete pass can leave nodes behind
            if MEL.get_num_material_expressions(material) == 0:
                break
            MEL.delete_all_material_expressions(material)
        return material
    return TOOLS.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def node(material, cls, x, y, **props):
    e = MEL.create_material_expression(material, getattr(unreal, "MaterialExpression" + cls), x, y)
    for key, value in props.items():
        e.set_editor_property(key, value)
    return e


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("link failed %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def build_litter_material(texture):
    """Paper and ash: masked and lit, so a sheet in a shadowed street is dark and one in the sun is bright. Masked
    sprites go through the base pass: no blending, no sorting, depth-tested overdraw only."""
    m = fresh_material("M_BerlinAmbient_Litter")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_niagara_sprites", True)
    m.set_editor_property("opacity_mask_clip_value", 0.4)
    sample = node(m, "ParticleSubUV", -700, 0, texture=texture, blend=False)
    color = node(m, "ParticleColor", -700, 300)
    base = node(m, "Multiply", -400, 0)
    link(sample, "RGB", base, "A")
    link(color, "RGB", base, "B")
    mask = node(m, "Multiply", -400, 250)
    link(sample, "A", mask, "A")
    link(color, "A", mask, "B")
    rough = node(m, "Constant", -400, 450, r=0.9)
    spec = node(m, "Constant", -400, 550, r=0.15)
    # a camera-facing sprite has a camera-facing normal: seen against the sun every sheet went black. A fixed world-up
    # normal lights the litter like the ground it blows over, from every view.
    m.set_editor_property("tangent_space_normal", False)
    up = node(m, "Constant3Vector", -400, 650, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
    MEL.connect_material_property(up, "", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.recompile_material(m)
    save(m)
    return m


def build_wisp_material():
    """Low dust wisps: lit translucency in the cheapest lit mode (volumetric non-directional), soft against the ground."""
    m = fresh_material("M_BerlinAmbient_Wisp")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_niagara_sprites", True)
    sample = node(m, "ParticleSubUV", -900, 0, texture=unreal.load_asset(PUFFS), blend=False)
    color = node(m, "ParticleColor", -900, 300)
    alpha = node(m, "Multiply", -600, 250)
    link(sample, "A", alpha, "A")
    link(color, "A", alpha, "B")
    fade = node(m, "DepthFade", -400, 250, fade_distance_default=60.0)
    if not MEL.connect_material_expressions(alpha, "", fade, "Opacity"):
        link(alpha, "", fade, "InOpacity")
    MEL.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    save(m)
    return m


# ---- effect type -------------------------------------------------------------------------------------------------------
def build_effect_type():
    """Distance culling for every Berlin ambient Niagara component: beyond CULL_DISTANCE an instance sleeps (no tick,
    no draw) and wakes when the camera returns."""
    if ASSETS.does_asset_exist(EFFECT_TYPE):
        effect_type = unreal.load_asset(EFFECT_TYPE)
    else:
        effect_type = TOOLS.create_asset("NET_BerlinAmbient", FOLDER, unreal.NiagaraEffectType, unreal.NiagaraEffectTypeFactoryNew())
    try:
        effect_type.set_editor_property("update_frequency", unreal.NiagaraScalabilityUpdateFrequency.MEDIUM)
        effect_type.set_editor_property("cull_reaction", unreal.NiagaraCullReaction.DEACTIVATE_IMMEDIATE_RESUME)
        settings = effect_type.get_editor_property("system_scalability_settings")
        log("effect type settings before: " + settings.export_text())
        text = ("(Settings=((Platforms=(QualityLevelMask=31),bCullByDistance=True,bCullMaxInstanceCount=False,"
                "bCullPerSystemMaxInstanceCount=False,MaxDistance=%f,bCullByMaxTimeWithoutRender_DEPRECATED=False,"
                "VisibilityCulling=(bCullWhenNotRendered=False,bCullByViewFrustum=False))))" % CULL_DISTANCE)
        settings.import_text(text)
        effect_type.set_editor_property("system_scalability_settings", settings)
        log("effect type settings after: " + effect_type.get_editor_property("system_scalability_settings").export_text())
    except Exception as error:  # noqa: BLE001
        log("EFFECT TYPE NOT CONFIGURED: %s" % error)
    save(effect_type)
    return effect_type


# ---- the Niagara recipe (import text, as Source/Tools/ImpactVfx/make_recipe.py writes it) -------------------------------
def num(value):
    return "%.6g" % float(value)


def frange(low, high=None):
    high = low if high is None else high
    if low == high:
        return "(Mode=UniformConstant,ChannelConstantsAndRanges=(%s),Min=%s,Max=%s)" % (num(low), num(low), num(high))
    return "(Mode=UniformRange,ChannelConstantsAndRanges=(%s,%s),Min=%s,Max=%s)" % (num(low), num(high), num(low), num(high))


def irange(low, high):
    return "(Mode=UniformRange,Min=%d,Max=%d)" % (low, high) if low != high else "(Mode=UniformConstant,Min=%d,Max=%d)" % (low, high)


def v3(xyz):
    return "(X=%s,Y=%s,Z=%s)" % tuple(num(v) for v in xyz)


def v3range(low, high):
    channels = ",".join(num(v) for v in list(low) + list(high))
    return "(Mode=NonUniformRange,ChannelConstantsAndRanges=(%s),Min=%s,Max=%s)" % (channels, v3(low), v3(high))


def v2range(low, high):
    return "(Mode=UniformRange,ChannelConstantsAndRanges=(%s,%s),Min=(X=%s,Y=%s),Max=(X=%s,Y=%s))" % (
        num(low), num(high), num(low), num(low), num(high), num(high))


def rgba(c):
    return "(R=%s,G=%s,B=%s,A=%s)" % tuple(num(v) for v in c)


def color_range(low, high):
    channels = ",".join(num(v) for v in list(low) + list(high))
    return "(Mode=NonUniformRange,ChannelConstantsAndRanges=(%s),Min=%s,Max=%s)" % (channels, rgba(low), rgba(high))


def curve(keys):
    return "(Keys=(%s))" % ",".join("(Time=%s,Value=%s)" % (num(t), num(v)) for t, v in keys)


def uniform_curve(keys):
    return "(Mode=UniformCurve,ChannelCurves=(%s))" % curve(keys)


def color_curve(rgb_keys, alpha_keys):
    return "(Mode=NonUniformCurve,ChannelCurves=(%s))" % ",".join([curve(rgb_keys)] * 3 + [curve(alpha_keys)])


def spawn_infos(rate, bursts):
    """A steady trickle plus the gust: a few bursts close together once per loop."""
    infos = ["(Type=Rate,Rate=%s,bEnabled=True)" % frange(rate)]
    infos += ["(Type=Burst,SpawnTime=%s,Amount=%s,bEnabled=True)" % (num(t), irange(lo, hi)) for t, lo, hi in bursts]
    return "(%s)" % ",".join(infos)


def layer(name, material, sub_image, frames, rate, bursts, loop, lifetime, size, color_low, color_high, speed, lift,
          curl, rotation_rate, size_curve, alpha_curve, height, sub_uv, sort=False):
    travel = speed[1] * lifetime[1]
    return {
        "name": name, "from": FOUNTAIN, "emitter": "Fountain",
        "emitter_properties": {
            # every stretch draws its own loop length, so the gusts of neighbouring stretches drift apart
            "EmitterState": "(LoopBehavior=Infinite,LoopDurationMode=Fixed,LoopDuration=%s,bRecalculateDurationEachLoop=True,"
                            "bEnableDistanceCulling=True,bMaxDistanceEnabled=True,MaxDistance=%s,"
                            "MaxDistanceReaction=SleepAndLetParticlesFinish,bResetAgeOnAwaken=True)" % (
                                frange(*loop), num(CULL_DISTANCE)),
            "SpawnInfos": spawn_infos(rate, bursts),
            "FixedBounds": "(Min=(X=%s,Y=%s,Z=-100),Max=(X=%s,Y=%s,Z=%s),IsValid=True)" % (
                num(-STRETCH_LENGTH / 2 - 200), num(-STRETCH_WIDTH / 2 - 500), num(STRETCH_LENGTH / 2 + travel),
                num(STRETCH_WIDTH / 2 + 500), num(height[1] + lift[1] * lifetime[1] + 300)),
        },
        "modules": {
            "InitializeParticle": {
                "LifetimeDistribution": frange(*lifetime),
                "SpriteSizeDistribution": v2range(*size),
                "SpriteRotationDistribution": frange(0, 360),
                "ColorDistribution": color_range(color_low, color_high),
                "InitialPositionDistribution": "(Mode=NonUniformConstant,ChannelConstantsAndRanges=(0,0,%s),Min=(X=0,Y=0,Z=%s),Max=(X=0,Y=0,Z=%s))" % (
                    num((height[0] + height[1]) / 2), num((height[0] + height[1]) / 2), num((height[0] + height[1]) / 2)),
            },
            "ShapeLocation": {"ShapePrimitive": "Box",
                              "BoxSize": v3((STRETCH_LENGTH, STRETCH_WIDTH, height[1] - height[0]))},
            "AddVelocity": {"VelocityType": "Linear", "CoordinateSpace": "Local",
                            "LinearVelocityDistribution": v3range((speed[0], -speed[0] * 0.18, lift[0]),
                                                                  (speed[1], speed[0] * 0.18, lift[1]))},
            "Drag": {"DragDistribution": frange(0.0)},
            "GravityForce": {"bModuleEnabled": "False"},
            "CurlNoiseForce": {"bModuleEnabled": "True" if curl > 0 else "False", "NoiseAmplitude": num(curl),
                               "NoiseFrequency": "0.6"},
            "SpriteRotationRate": {"bModuleEnabled": "True", "RotationRateDistribution": frange(*rotation_rate)},
            "ScaleColor": {"bModuleEnabled": "True", "ScaleDistribution": color_curve([(0, 1), (1, 1)], alpha_curve)},
            "ScaleSpriteSize": {"bModuleEnabled": "True", "ScaleDistribution": uniform_curve(size_curve)},
            "SubUVAnimation": dict({"bModuleEnabled": "True", "NumFrames": str(frames)}, **sub_uv),
        },
        "renderer": {
            "Material": material + "." + material.rsplit("/", 1)[1],
            "SubImageSize": "(X=%d,Y=%d)" % sub_image,
            "bSubImageBlend": "False",
            "Alignment": "Unaligned", "FacingMode": "FaceCamera",
            "bCastShadows": "False",
            "SortMode": "ViewDistance" if sort else "None",
        },
    }


def write_recipe():
    litter = FOLDER + "/M_BerlinAmbient_Litter"
    wisp = FOLDER + "/M_BerlinAmbient_Wisp"
    grow_shrink = [(0, 0.0), (0.06, 1.0), (0.9, 1.0), (1, 0.0)]     # masked sprites cannot fade: they grow in and shrink out
    layers = [
        # paper: frames 0-11 of the atlas tumble in a loop; about 14 sheets alive per stretch, 30 in a gust
        layer("Paper", litter, (4, 4), 16, rate=1.8, bursts=[(2.0, 5, 8), (2.5, 4, 7), (3.2, 2, 4)], loop=(11, 17),
              lifetime=(7, 11), size=(30, 46), color_low=(0.62, 0.60, 0.55, 1), color_high=(0.95, 0.93, 0.86, 1),
              speed=(170, 430), lift=(-4, 14), curl=140, rotation_rate=(-150, 150), size_curve=grow_shrink,
              alpha_curve=[(0, 1), (1, 1)], height=(12, 130),
              sub_uv={"AnimationMode": "InfiniteLoop", "LoopsPerSecond": "0.8", "bEndFrameRangeOverride_Enabled": "True",
                      "EndFrameRangeOverride": "11"}),
        # ash: frames 12-15, picked at random; small, many, slower and higher
        layer("Ash", litter, (4, 4), 16, rate=5.0, bursts=[(2.1, 14, 20), (2.8, 8, 12)], loop=(11, 17),
              lifetime=(5, 9), size=(7, 14), color_low=(0.10, 0.10, 0.10, 1), color_high=(0.75, 0.74, 0.72, 1),
              speed=(110, 320), lift=(0, 38), curl=220, rotation_rate=(-260, 260), size_curve=grow_shrink,
              alpha_curve=[(0, 1), (1, 1)], height=(10, 320),
              sub_uv={"AnimationMode": "Random", "RandomChangeInterval": "100", "bStartFrameRangeOverride_Enabled": "True",
                      "StartFrameRangeOverride": "12", "bEndFrameRangeOverride_Enabled": "True",
                      "EndFrameRangeOverride": "15"}),
        # wisps: a few big soft sprites hugging the ground, the only translucent layer
        layer("Wisp", wisp, (2, 2), 4, rate=0.32, bursts=[(1.9, 1, 2)], loop=(11, 17),
              lifetime=(7, 10), size=(260, 430), color_low=(0.36, 0.35, 0.33, 1), color_high=(0.50, 0.48, 0.45, 1),
              speed=(220, 380), lift=(2, 10), curl=0, rotation_rate=(-12, 12),
              size_curve=[(0, 0.55), (1, 1.5)], alpha_curve=[(0, 0.0), (0.25, 0.34), (0.7, 0.22), (1, 0.0)],
              height=(40, 110),
              sub_uv={"AnimationMode": "Random", "RandomChangeInterval": "100"}, sort=False),
    ]
    recipe = {"systems": [{"target": SYSTEM, "base": FOUNTAIN.split(".")[0], "stateless": layers, "disable": ["Fountain"]}]}
    os.makedirs(os.path.dirname(RECIPE_FILE), exist_ok=True)
    with open(RECIPE_FILE, "w") as handle:
        json.dump(recipe, handle, indent=1)
    return RECIPE_FILE


def build_system(effect_type):
    recipe = write_recipe()
    unreal.SystemLibrary.execute_console_command(None, "RTS.ImpactVfx.BuildFromRecipe " + recipe)
    system = unreal.load_asset(SYSTEM)
    if system is None:
        log("SYSTEM NOT BUILT")
        return None
    try:
        system.set_editor_property("effect_type", effect_type)
        log("effect type on system: %s" % system.get_editor_property("effect_type").get_name())
    except Exception as error:  # noqa: BLE001
        log("EFFECT TYPE NOT SET ON SYSTEM: %s" % error)
    save(system)
    return system


def run():
    texture = import_texture("T_BerlinAmbient_Litter")
    build_litter_material(texture)
    build_wisp_material()
    effect_type = build_effect_type()
    build_system(effect_type)
    log("DONE")


run()
