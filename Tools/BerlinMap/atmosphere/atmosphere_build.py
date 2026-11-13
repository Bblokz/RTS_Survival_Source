"""Assets of the Ger_Berlin atmosphere per district (idempotent; Docs/Lighting/BerlinLighting.md, "Atmosphere per district").

Headless:
    sh Source/Tools/BerlinMap/atmosphere/run_headless.sh D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/atmosphere/atmosphere_build.py build
Inside an editor that already has the assets loaded (their files are locked for a second process), MATERIALS ONLY:
    import atmosphere_place, atmosphere_build
    atmosphere_place.clear()                    # never empty a material that placed actors are using, see fresh_material()
    atmosphere_build.run(systems=False)
The four Niagara systems must be rebuilt headless, with no editor holding them: rebuilt inside an editor that had them
loaded, their components never activated again (met 2026-11-13).

Run atmosphere_gen.py first: it writes the district mask this file imports and the mask rectangle the sheet material uses.

Builds, under /Game/RTS_Survival/Blueprints/Environment/Berlin/Atmosphere:
  MPC_BerlinAtmosphere        THE KNOBS: one scalar per district layer and three colours (existing values are kept on a rebuild)
  T_BerlinAtmo_Districts      district mask: R Mitte smoke, G crater mist, B east smoke
  M_BerlinAtmo_Sheet          unlit translucent ground-parallel sheet: drifting procedural smoke x mask, depth fade, sun shade
                              from MF_BerlinSmokePallField (only called, never changed), wind speed from MPC_BerlinSmokePall
  M_BerlinAtmo_Smoulder       soft sprite of the smouldering ruins, district colour from the mask (knob RuinSmoke)
  NS_BerlinAtmo_Smoulder_Wisp / _Shell / _Hero   one stateless emitter each: the core of the smoke layer, map-wide
  M_BerlinAtmo_MitteSmoke     soft sprite for the Mitte smoke banks       (knob MitteSmoke)
  M_BerlinAtmo_CraterVapour   soft sprite for the green vapour            (knob CraterGlow)
  M_BerlinAtmo_EastColumn     soft sprite for the smoke columns, fades out near the camera (knob EastSmoke)
  M_BerlinAtmo_ShimmerCard    heat shimmer on ground-parallel cards: refraction only (knob CraterShimmer)
  M_LF_BerlinAtmo_Flame       light function: flicker                      (knob EastFire); instances MI_..._0.._5 differ in phase
  M_LF_BerlinAtmo_Crater      light function: slow pulse                   (knob CraterGlow)
  NET_BerlinAtmosphere        effect type: an instance further than CULL_DISTANCE from the camera sleeps
  NS_BerlinAtmo_MitteSmoke    two stateless emitters (Bank, Dust)
  NS_BerlinAtmo_CraterVapour  one stateless emitter (green vapour)
  NS_BerlinAtmo_Fire_Ruin     copy of N_RTS_Fire05_DitherAA without its smoke + stateless Column
  NS_BerlinAtmo_Fire_Wreck    the same with a smaller Column
Nothing here uses the engine's height fog, volumetric fog or fog volumes. No particle lights, no collision.
"""
import json

import unreal

FOLDER = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Atmosphere"
SAVED = "D:/UE5Projects/RTS_Survival/Saved/Berlin/"
PLACEMENT_FILE = SAVED + "terrain/Ger_Berlin_atmosphere.json"
TEXTURE_DIR = SAVED + "atmosphere/textures/"
RECIPE_FILE = SAVED + "atmosphere/atmosphere_recipe.json"
FOUNTAIN = "/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight"
PUFFS = "/Game/RTS_Survival/Blueprints/Environment/AmbientFx/T_AmbientDust_Puffs"
SKY = "/Game/RTS_Survival/Materials/Sky/"
FIRE_RUIN_BASE = "/Game/RTS_Survival/SpecialEffects/Fire/N_RTS_Fire05_DitherAA"
FIRE_WRECK_BASE = FIRE_RUIN_BASE      # the engine fire (RTS_EngineFire_0) is too small to read from the game camera
MPC = FOLDER + "/MPC_BerlinAtmosphere"
EFFECT_TYPE = FOLDER + "/NET_BerlinAtmosphere"
CULL_DISTANCE = 15000.0     # cm; the game camera's far view corner is about 75 m away, a smoke column leans 40 m further
LIGHT_PHASES = 6

# THE KNOBS (defaults; a rebuild keeps the values the collection already holds)
KNOB_SCALARS = [("RuinSmoke", 1.0), ("MitteSmoke", 1.0), ("CraterGlow", 0.6), ("CraterShimmer", 1.0), ("EastSmoke", 1.0), ("EastFire", 1.0),
                ("HazeSheets", 1.0)]
KNOB_COLOURS = [("MitteColor", (0.27, 0.26, 0.24, 1.0)), ("CraterColor", (0.20, 0.62, 0.15, 1.0)),
                ("EastSmokeColor", (0.055, 0.053, 0.05, 1.0)), ("RuinSmokeColor", (0.10, 0.097, 0.093, 1.0))]

# look constants baked into the materials (change here, rebuild)
SHEET_OPACITY = dict(mitte=0.40, crater=0.05, east=0.42)      # opacity of a sheet where its mask and its smoke are both 1
SHEET_DEPTH_FADE = 520.0        # cm along the view ray: what stands up out of the haze (a unit, a wall top) is hazed less
SHEET_BANK_SIZE = 4200.0        # cm, the large smoke banks; the wisps inside them are about a third of this
CRATER_GLOW_BOOST = 1.25        # the crater mist is emissive: its colour is this much brighter than the knob colour
SHIMMER_STRENGTH = 0.12         # refraction strength minus one at knob 1 (pixel-normal-offset); 0.085 wavers faintly, 3 is a lens
FIRE_LIGHT_HEADROOM = 0.5       # the light function outputs knob x this, so the knob can go up to 2

MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
WIND = (0.2149, 0.9766)


def log(text):
    unreal.log_warning("ATM_BUILD " + str(text))


def save(asset):
    ok = ASSETS.save_loaded_asset(asset, False)
    log("saved %s %s" % (asset.get_path_name().split(".")[0], ok))
    return ok


def placement():
    with open(PLACEMENT_FILE, "r") as handle:
        return json.load(handle)


# ---- the knobs -------------------------------------------------------------------------------------------------------------
def build_collection(reset=False):
    """Creates the collection or adds what is missing. Existing values are the owner's tuning and are kept, unless reset."""
    if ASSETS.does_asset_exist(MPC):
        collection = unreal.load_asset(MPC)
        fresh = False
    else:
        fresh = True
        collection = TOOLS.create_asset("MPC_BerlinAtmosphere", FOLDER, unreal.MaterialParameterCollection,
                                        unreal.MaterialParameterCollectionFactoryNew())
    scalars = list(collection.get_editor_property("scalar_parameters"))
    have = {str(p.get_editor_property("parameter_name")) for p in scalars}
    changed = bool([n for n, _ in KNOB_SCALARS if n not in have])
    if reset:
        for parameter in scalars:
            parameter.set_editor_property("default_value", dict(KNOB_SCALARS)[str(parameter.get_editor_property("parameter_name"))])
    for name, value in KNOB_SCALARS:
        if name not in have:
            parameter = unreal.CollectionScalarParameter()
            parameter.set_editor_property("parameter_name", name)
            parameter.set_editor_property("default_value", value)
            scalars.append(parameter)
    collection.set_editor_property("scalar_parameters", scalars)
    vectors = list(collection.get_editor_property("vector_parameters"))
    have = {str(p.get_editor_property("parameter_name")) for p in vectors}
    if reset:
        for parameter in vectors:
            parameter.set_editor_property("default_value", unreal.LinearColor(*dict(KNOB_COLOURS)[str(parameter.get_editor_property("parameter_name"))]))
    for name, value in KNOB_COLOURS:
        if name not in have:
            parameter = unreal.CollectionVectorParameter()
            parameter.set_editor_property("parameter_name", name)
            parameter.set_editor_property("default_value", unreal.LinearColor(*value))
            vectors.append(parameter)
    collection.set_editor_property("vector_parameters", vectors)
    if changed or reset or fresh:
        save(collection)        # untouched otherwise: an editor that has the map open holds this file
    log("knobs: %s" % {str(p.get_editor_property("parameter_name")): round(p.get_editor_property("default_value"), 3)
                       for p in collection.get_editor_property("scalar_parameters")})
    return collection


# ---- texture ---------------------------------------------------------------------------------------------------------------
def import_mask(name):
    task = unreal.AssetImportTask()
    task.filename = TEXTURE_DIR + name + ".png"
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + name)
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)
    save(texture)
    return texture


# ---- material helpers --------------------------------------------------------------------------------------------------------
def fresh_material(name):
    """Empties an existing material and returns it. NEVER on a material that spawned actors are using (the editor
    asserted on that once, see the project notes): clear the ATM_ actors first, or run headless."""
    path = FOLDER + "/" + name
    if ASSETS.does_asset_exist(path):
        material = unreal.load_asset(path)
        for _ in range(20):
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


def out(a, a_out, material_property):
    if not MEL.connect_material_property(a, a_out, material_property):
        raise RuntimeError("output link failed %s -> %s" % (a.get_name(), material_property))


def knob(material, collection, name, x, y):
    e = node(material, "CollectionParameter", x, y)
    e.set_editor_property("collection", collection)
    e.set_editor_property("parameter_name", name)
    return e


def knob_colour(material, collection, name, x, y):
    e = knob(material, collection, name, x, y)
    mask = node(material, "ComponentMask", x + 220, y, r=True, g=True, b=True, a=False)
    link(e, "", mask, "")
    return mask


def custom(material, x, y, code, inputs, output_type, description):
    e = node(material, "Custom", x, y)
    entries = []
    for name in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        entries.append(entry)
    e.set_editor_property("inputs", entries)
    e.set_editor_property("output_type", output_type)
    e.set_editor_property("code", code)
    e.set_editor_property("description", description)
    return e


def depth_fade(material, x, y, source, distance):
    fade = node(material, "DepthFade", x, y, fade_distance_default=distance)
    if not MEL.connect_material_expressions(source, "", fade, "Opacity"):
        link(source, "", fade, "InOpacity")
    return fade


NOISE_STRUCT = '''
struct FBerlinAtmoNoise
{
    float Hash(float2 Cell) { return frac(sin(dot(Cell, float2(127.1, 311.7))) * 43758.5453); }
    float Value(float2 Q)
    {
        float2 Cell = floor(Q);
        float2 F = frac(Q);
        F = F * F * (3.0 - 2.0 * F);
        return lerp(lerp(Hash(Cell), Hash(Cell + float2(1.0, 0.0)), F.x),
                    lerp(Hash(Cell + float2(0.0, 1.0)), Hash(Cell + float2(1.0, 1.0)), F.x), F.y);
    }
    float Soft2(float2 Q) { return Value(Q) * 0.64 + Value(Q * 2.07 + 17.3) * 0.36; }
};
FBerlinAtmoNoise N;
'''

SHEET_CODE = NOISE_STRUCT + '''
// P world position, T seconds, M district mask (R Mitte, G crater, B east), KM KC KE the knobs, WS wind speed cm/s,
// Sun the sunlight multiplier of the sun's own light function at P, CM CC CE the three colours.
float2 WindDir = normalize(float2(0.22, 1.0));
float2 Across = float2(-WindDir.y, WindDir.x);
float Lift = P.z * 0.004;                                   // every sheet height gets its own pattern and a little more speed
float2 G1 = P.xy - T * WS * (1.0 + 0.2 * Lift) * WindDir;
float2 U1 = float2(dot(G1, WindDir), dot(G1, Across) * 1.5) / BANK_SIZE + Lift * 7.3;
float2 G2 = P.xy - T * WS * 1.7 * WindDir;
float2 U2 = float2(dot(G2, WindDir), dot(G2, Across) * 1.9) / (BANK_SIZE * 0.34) + Lift * 3.1 + 31.7;
float Bank = N.Soft2(U1);
float Wisp = N.Soft2(U2);
float Body = Bank * 0.72 + Wisp * 0.28;
float Light = lerp(0.42, 1.0, saturate(Sun));
float AM = M.r * KM * smoothstep(0.40, 0.70, Body) * lerp(0.6, 1.0, Wisp) * OP_MITTE;
float AE = M.b * KE * smoothstep(0.36, 0.68, Body) * lerp(0.6, 1.0, Wisp) * OP_EAST;
float Pulse = 0.86 + 0.14 * sin(T * 0.55 + Bank * 6.0);
float AC = M.g * KC * (0.5 + 0.5 * Wisp) * Pulse * OP_CRATER;
float Sum = AM + AE + AC;
float3 Colour = (CM * Light * AM + CE * Light * AE + CC * GLOW_BOOST * AC) / max(Sum, 0.0001);
return float4(Colour, saturate(Sum));
'''


def build_sheet_material(collection, mask_texture, rect):
    m = fresh_material("M_BerlinAtmo_Sheet")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("use_translucency_vertex_fog", False)
    pall = unreal.load_asset(SKY + "MPC_BerlinSmokePall")
    field = unreal.load_asset(SKY + "MF_BerlinSmokePallField")
    position = node(m, "WorldPosition", -1700, 0)
    time = node(m, "Time", -1700, 200)
    # mask UV from world XY: image columns run along world X, rows along world Y
    xy = node(m, "ComponentMask", -1500, 350, r=True, g=True, b=False, a=False)
    link(position, "", xy, "")
    # MaskRect = (x min, y min, x size, y size) of the mask in the world; a parameter only so that a test instance can lay
    # the mask over another map. The default is Ger_Berlin's rectangle from the placement file.
    mask_rect = node(m, "VectorParameter", -1750, 500, parameter_name="MaskRect",
                     default_value=unreal.LinearColor(rect[0], rect[2], rect[1] - rect[0], rect[3] - rect[2]))
    origin = node(m, "ComponentMask", -1500, 450, r=True, g=True, b=False, a=False)
    link(mask_rect, "", origin, "")
    size = node(m, "AppendVector", -1500, 550)
    link(mask_rect, "B", size, "A")
    link(mask_rect, "A", size, "B")
    sub = node(m, "Subtract", -1300, 400)
    link(xy, "", sub, "A")
    link(origin, "", sub, "B")
    div = node(m, "Divide", -1150, 400)
    link(sub, "", div, "A")
    link(size, "", div, "B")
    sample = node(m, "TextureSample", -950, 400, texture=mask_texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(div, "", sample, "UVs")
    # the sun's own cloud field, so the haze is dim under a cloud shadow like the ground below it
    call = node(m, "MaterialFunctionCall", -1300, 700)
    call.set_material_function(field)
    link(position, "", call, "Position")
    sun = node(m, "ComponentMask", -1050, 700, r=True, g=False, b=False, a=False)
    link(call, "CloudField", sun, "")
    drift = knob(m, pall, "RayDetailDrift", -1300, 900)
    drift_scale = knob(m, pall, "CloudDriftScale", -1300, 1000)
    speed = node(m, "Multiply", -1050, 950)
    link(drift, "", speed, "A")
    link(drift_scale, "", speed, "B")
    code = SHEET_CODE.replace("BANK_SIZE", "%.1f" % SHEET_BANK_SIZE).replace("OP_MITTE", "%.3f" % SHEET_OPACITY["mitte"]) \
        .replace("OP_EAST", "%.3f" % SHEET_OPACITY["east"]).replace("OP_CRATER", "%.3f" % SHEET_OPACITY["crater"]) \
        .replace("GLOW_BOOST", "%.2f" % CRATER_GLOW_BOOST)
    c = custom(m, -500, 300, code, ["P", "T", "M", "KM", "KC", "KE", "WS", "Sun", "CM", "CC", "CE"],
               unreal.CustomMaterialOutputType.CMOT_FLOAT4, "Berlin atmosphere sheet: drifting smoke x district mask")
    link(position, "", c, "P")
    link(time, "", c, "T")
    link(sample, "RGB", c, "M")
    link(knob(m, collection, "MitteSmoke", -900, 1100), "", c, "KM")
    link(knob(m, collection, "CraterGlow", -900, 1200), "", c, "KC")
    link(knob(m, collection, "EastSmoke", -900, 1300), "", c, "KE")
    link(speed, "", c, "WS")
    link(sun, "", c, "Sun")
    link(knob_colour(m, collection, "MitteColor", -900, 1400), "", c, "CM")
    link(knob_colour(m, collection, "CraterColor", -900, 1500), "", c, "CC")
    link(knob_colour(m, collection, "EastSmokeColor", -900, 1600), "", c, "CE")
    rgb = node(m, "ComponentMask", -200, 250, r=True, g=True, b=True, a=False)
    link(c, "", rgb, "")
    alpha = node(m, "ComponentMask", -200, 400, r=False, g=False, b=False, a=True)
    link(c, "", alpha, "")
    sheets = node(m, "Multiply", -50, 480)          # HazeSheets: the one knob that turns the optional sheets down or off
    link(alpha, "", sheets, "A")
    link(knob(m, collection, "HazeSheets", -300, 560), "", sheets, "B")
    fade = depth_fade(m, 100, 400, sheets, SHEET_DEPTH_FADE)
    out(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(fade, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    save(m)
    return m


def build_puff_material(name, collection, knob_name, colour_name, fade_distance, camera_fade=None, boost=1.0):
    """Soft unlit sprite on the project's dust puff sheet. Colour = particle colour x the knob colour; opacity = puff x
    particle alpha x the knob, faded against the ground (and near the camera for the columns that rise to its height)."""
    m = fresh_material(name)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_niagara_sprites", True)
    m.set_editor_property("use_translucency_vertex_fog", False)
    # T_AmbientDust_Puffs: 2x2 puffs, R = density, G = the darker inner billow
    sample = node(m, "ParticleSubUV", -1100, 0, texture=unreal.load_asset(PUFFS), blend=False,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    particle = node(m, "ParticleColor", -1100, 300)
    tinted = node(m, "Multiply", -900, 100)
    link(particle, "RGB", tinted, "A")
    link(knob_colour(m, collection, colour_name, -1100, 500), "", tinted, "B")
    shade = node(m, "LinearInterpolate", -900, -100, const_a=1.0, const_b=0.72)
    link(sample, "G", shade, "Alpha")
    colour = node(m, "Multiply", -700, 100)
    link(tinted, "", colour, "A")
    link(shade, "", colour, "B")
    if boost != 1.0:
        boosted = node(m, "Multiply", -500, 100, const_b=boost)
        link(colour, "", boosted, "A")
        colour = boosted
    alpha = node(m, "Multiply", -700, 300)
    link(sample, "R", alpha, "A")
    link(particle, "A", alpha, "B")
    scaled = node(m, "Multiply", -500, 300)
    link(alpha, "", scaled, "A")
    link(knob(m, collection, knob_name, -700, 450), "", scaled, "B")
    last = scaled
    if camera_fade is not None:
        depth = node(m, "PixelDepth", -700, 600)
        near = node(m, "Subtract", -500, 600, const_b=camera_fade[0])
        link(depth, "", near, "A")
        ramp = node(m, "Divide", -350, 600, const_b=camera_fade[1] - camera_fade[0])
        link(near, "", ramp, "A")
        clamp = node(m, "Saturate", -200, 600)
        link(ramp, "", clamp, "")
        faded = node(m, "Multiply", -200, 350)
        link(scaled, "", faded, "A")
        link(clamp, "", faded, "B")
        last = faded
    sat = node(m, "Saturate", -50, 350)
    link(last, "", sat, "")
    fade = depth_fade(m, 100, 350, sat, fade_distance)
    out(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(fade, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    save(m)
    return m


SMOULDER_CODE = '''
// the smoke of a smouldering ruin takes its district's colour from the mask at the particle: M (R Mitte, G crater, B east)
float3 C = lerp(CR, CE, saturate(M.b * 1.6));
C = lerp(C, CR * 0.6 + CC * 0.22, saturate(M.g * 1.2));
return C;
'''


def build_smoulder_material(collection, mask_texture, rect):
    """Soft unlit sprite of the smouldering ruins (knob RuinSmoke). Colour by district from the mask, fades near the camera."""
    m = fresh_material("M_BerlinAtmo_Smoulder")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_niagara_sprites", True)
    m.set_editor_property("use_translucency_vertex_fog", False)
    sample = node(m, "ParticleSubUV", -1100, 0, texture=unreal.load_asset(PUFFS), blend=False,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    particle = node(m, "ParticleColor", -1100, 300)
    position = node(m, "WorldPosition", -1700, 700)
    xy = node(m, "ComponentMask", -1500, 700, r=True, g=True, b=False, a=False)
    link(position, "", xy, "")
    sub = node(m, "Subtract", -1350, 700)
    link(xy, "", sub, "A")
    link(node(m, "Constant2Vector", -1500, 800, r=rect[0], g=rect[2]), "", sub, "B")
    div = node(m, "Divide", -1200, 700)
    link(sub, "", div, "A")
    link(node(m, "Constant2Vector", -1500, 900, r=rect[1] - rect[0], g=rect[3] - rect[2]), "", div, "B")
    mask = node(m, "TextureSample", -1050, 700, texture=mask_texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(div, "", mask, "UVs")
    c = custom(m, -700, 700, SMOULDER_CODE, ["M", "CR", "CE", "CC"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "district colour")
    link(mask, "RGB", c, "M")
    link(knob_colour(m, collection, "RuinSmokeColor", -1050, 950), "", c, "CR")
    link(knob_colour(m, collection, "EastSmokeColor", -1050, 1050), "", c, "CE")
    link(knob_colour(m, collection, "CraterColor", -1050, 1150), "", c, "CC")
    tinted = node(m, "Multiply", -500, 100)
    link(particle, "RGB", tinted, "A")
    link(c, "", tinted, "B")
    shade = node(m, "LinearInterpolate", -900, -100, const_a=1.0, const_b=0.72)
    link(sample, "G", shade, "Alpha")
    colour = node(m, "Multiply", -300, 100)
    link(tinted, "", colour, "A")
    link(shade, "", colour, "B")
    alpha = node(m, "Multiply", -700, 300)
    link(sample, "R", alpha, "A")
    link(particle, "A", alpha, "B")
    scaled = node(m, "Multiply", -500, 300)
    link(alpha, "", scaled, "A")
    link(knob(m, collection, "RuinSmoke", -700, 450), "", scaled, "B")
    depth = node(m, "PixelDepth", -700, 550)
    near = node(m, "Subtract", -550, 550, const_b=500.0)
    link(depth, "", near, "A")
    ramp = node(m, "Divide", -400, 550, const_b=1400.0)
    link(near, "", ramp, "A")
    clamp = node(m, "Saturate", -250, 550)
    link(ramp, "", clamp, "")
    faded = node(m, "Multiply", -200, 350)
    link(scaled, "", faded, "A")
    link(clamp, "", faded, "B")
    sat = node(m, "Saturate", -50, 350)
    link(faded, "", sat, "")
    fade = depth_fade(m, 100, 350, sat, 140.0)
    out(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(fade, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    save(m)
    return m


SHIMMER_CODE = NOISE_STRUCT + '''
// P world position, T seconds, UV card coordinates 0..1. Cells of about 2.6 m drift with the wind; the shimmer comes and
// goes in patches and fades to nothing at the card's edge.
float2 W = normalize(float2(0.22, 1.0));
float2 Q = P.xy / 260.0 - T * 0.35 * W;
float2 Bend = float2(N.Soft2(Q), N.Soft2(Q + 19.7)) - 0.5;
float Patch = smoothstep(0.32, 0.68, N.Soft2(P.xy / 1400.0 + T * 0.05));
float Round = saturate(1.0 - length(UV - 0.5) * 2.0);
Round = Round * Round * (3.0 - 2.0 * Round);
return float3(Bend * 2.0, Round * Patch);
'''


def build_shimmer_material(collection):
    """Heat shimmer on ground-parallel CARDS (static meshes): draws nothing itself, only bends what is behind it
    (refraction by pixel normal offset). Not a sprite material: on the stateless Niagara sprites the same graph bent
    nothing at any strength, on a card it does (tested side by side 2026-11-13). Coverage must be ignored, because the
    card's opacity is 0 and by default a surface without coverage refracts nothing."""
    m = fresh_material("M_BerlinAtmo_ShimmerCard")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("use_translucency_vertex_fog", False)
    m.set_editor_property("refraction_method", unreal.RefractionMode.RM_PIXEL_NORMAL_OFFSET)
    m.set_editor_property("refraction_coverage_mode", unreal.RefractionCoverageMode.RCM_COVERAGE_IGNORED)
    position = node(m, "WorldPosition", -1100, -100)
    uv = node(m, "TextureCoordinate", -1100, 0)
    time = node(m, "Time", -1100, 150)
    c = custom(m, -700, 100, SHIMMER_CODE, ["P", "T", "UV"], unreal.CustomMaterialOutputType.CMOT_FLOAT3,
               "Berlin heat shimmer: bend direction xy, mask z")
    link(position, "", c, "P")
    link(time, "", c, "T")
    link(uv, "", c, "UV")
    bend = node(m, "ComponentMask", -400, 0, r=True, g=True, b=False, a=False)
    link(c, "", bend, "")
    mask = node(m, "ComponentMask", -400, 200, r=False, g=False, b=True, a=False)
    link(c, "", mask, "")
    bend_masked = node(m, "Multiply", -200, 0)
    link(bend, "", bend_masked, "A")
    link(mask, "", bend_masked, "B")
    normal = node(m, "AppendVector", 0, 0)
    one = node(m, "Constant", -200, 120, r=1.0)
    link(bend_masked, "", normal, "A")
    link(one, "", normal, "B")
    strength = node(m, "Multiply", -200, 400, const_b=SHIMMER_STRENGTH)
    link(knob(m, collection, "CraterShimmer", -500, 400), "", strength, "A")
    masked_strength = node(m, "Multiply", 0, 400)
    link(strength, "", masked_strength, "A")
    link(mask, "", masked_strength, "B")
    refraction = node(m, "Add", 150, 400, const_b=1.0)
    link(masked_strength, "", refraction, "A")
    zero = node(m, "Constant", 0, 600, r=0.0)
    black = node(m, "Constant3Vector", 0, 700, constant=unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    out(normal, "", unreal.MaterialProperty.MP_NORMAL)
    out(refraction, "", unreal.MaterialProperty.MP_REFRACTION)
    out(zero, "", unreal.MaterialProperty.MP_OPACITY)
    out(black, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    save(m)
    return m


FIRE_LF_CODE = '''
// A fire's light: two smooth random walks in time (about 3 and 7 changes a second), so the brightness swings by about
// a quarter around its mean, irregularly, never fully dark. (The first version summed three slow sines and a small
// 11 Hz jitter: about +-8 % that the eye and the frame blending averaged away.) Nothing ticks.
float t = T + Phase * 17.31;
float a = t * 3.1;
float ia = floor(a);
float fa = frac(a);
fa = fa * fa * (3.0 - 2.0 * fa);
float N1 = lerp(frac(sin(ia * 12.9898) * 43758.5453), frac(sin((ia + 1.0) * 12.9898) * 43758.5453), fa);
float b = t * 7.3 + 5.0;
float ib = floor(b);
float fb = frac(b);
fb = fb * fb * (3.0 - 2.0 * fb);
float N2 = lerp(frac(sin(ib * 78.233) * 43758.5453), frac(sin((ib + 1.0) * 78.233) * 43758.5453), fb);
float Flicker = 0.64 + 0.56 * (N1 - 0.5) + 0.24 * (N2 - 0.5);
return saturate(HEADROOM * K * Flicker);
'''

CRATER_LF_CODE = '''
float Pulse = 0.84 + 0.11 * sin(T * 0.55) + 0.05 * sin(T * 1.7 + 0.9);
return saturate(HEADROOM * K * Pulse);
'''


def build_light_function(name, collection, knob_name, code, with_phase):
    m = fresh_material(name)
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    time = node(m, "Time", -700, 0)
    inputs = ["T", "K"] + (["Phase"] if with_phase else [])
    c = custom(m, -300, 0, code.replace("HEADROOM", "%.2f" % FIRE_LIGHT_HEADROOM), inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT1, name)
    link(time, "", c, "T")
    link(knob(m, collection, knob_name, -700, 150), "", c, "K")
    if with_phase:
        phase = node(m, "ScalarParameter", -700, 300, parameter_name="Phase", default_value=0.0)
        link(phase, "", c, "Phase")
    out(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    save(m)
    return m


def build_fire_light_instances(parent):
    for index in range(LIGHT_PHASES):
        name = "MI_LF_BerlinAtmo_Flame_%d" % index
        path = FOLDER + "/" + name
        if ASSETS.does_asset_exist(path):
            instance = unreal.load_asset(path)
        else:
            instance = TOOLS.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(instance, parent)
        MEL.set_material_instance_scalar_parameter_value(instance, "Phase", float(index) * 1.618)
        MEL.update_material_instance(instance)
        save(instance)


# ---- effect type -----------------------------------------------------------------------------------------------------------
def build_effect_type():
    if ASSETS.does_asset_exist(EFFECT_TYPE):
        effect_type = unreal.load_asset(EFFECT_TYPE)
    else:
        effect_type = TOOLS.create_asset("NET_BerlinAtmosphere", FOLDER, unreal.NiagaraEffectType, unreal.NiagaraEffectTypeFactoryNew())
    try:
        effect_type.set_editor_property("update_frequency", unreal.NiagaraScalabilityUpdateFrequency.MEDIUM)
        # PAUSE, not deactivate: a stateless emitter that was deactivated by the distance cull never came back when the
        # camera returned (the smoke columns were gone on the second visit; tested 2026-11-13). Paused systems do not tick.
        effect_type.set_editor_property("cull_reaction", unreal.NiagaraCullReaction.PAUSE_RESUME)
        settings = effect_type.get_editor_property("system_scalability_settings")
        text = ("(Settings=((Platforms=(QualityLevelMask=31),bCullByDistance=True,bCullMaxInstanceCount=False,"
                "bCullPerSystemMaxInstanceCount=False,MaxDistance=%f,bCullByMaxTimeWithoutRender_DEPRECATED=False,"
                "VisibilityCulling=(bCullWhenNotRendered=False,bCullByViewFrustum=False))))" % CULL_DISTANCE)
        settings.import_text(text)
        effect_type.set_editor_property("system_scalability_settings", settings)
    except Exception as error:  # noqa: BLE001
        log("EFFECT TYPE NOT CONFIGURED: %s" % error)
    save(effect_type)
    return effect_type


# ---- Niagara recipes (import text, as Source/Tools/ImpactVfx/make_recipe.py writes it) ------------------------------------
def num(value):
    return "%.6g" % float(value)


def frange(low, high=None):
    high = low if high is None else high
    if low == high:
        return "(Mode=UniformConstant,ChannelConstantsAndRanges=(%s),Min=%s,Max=%s)" % (num(low), num(low), num(high))
    return "(Mode=UniformRange,ChannelConstantsAndRanges=(%s,%s),Min=%s,Max=%s)" % (num(low), num(high), num(low), num(high))


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


def color_curves(red, green, blue, alpha):
    return "(Mode=NonUniformCurve,ChannelCurves=(%s))" % ",".join(curve(k) for k in (red, green, blue, alpha))


def wind_velocity(speed_low, speed_high, spread, lift_low, lift_high):
    """A box of velocities along the wind (the systems are placed unrotated, so local space is world space)."""
    return v3range((WIND[0] * speed_low - spread, WIND[1] * speed_low, lift_low), (WIND[0] * speed_high + spread, WIND[1] * speed_high, lift_high))


def layer(name, material, rate, lifetime, size, color_low, color_high, velocity, box, centre_z, alpha_curve, size_curve,
          bounds, drag=0.0, acceleration=None, rotation_rate=(-5, 5), colour_curves=None, loop=(20, 20), sort=False):
    """One stateless sprite emitter. box = (x, y, z) spawn box, centred centre_z above the system; bounds = (min, max)."""
    unit = [(0, 1), (1, 1)]
    scale_colour = color_curves(*(colour_curves or (unit, unit, unit)), alpha_curve)
    modules = {
        "InitializeParticle": {
            "LifetimeDistribution": frange(*lifetime),
            "SpriteSizeDistribution": v2range(*size),
            "SpriteRotationDistribution": frange(0, 360),
            "ColorDistribution": color_range(color_low, color_high),
            "InitialPositionDistribution": "(Mode=NonUniformConstant,ChannelConstantsAndRanges=(0,0,%s),Min=(X=0,Y=0,Z=%s),Max=(X=0,Y=0,Z=%s))" % (
                num(centre_z), num(centre_z), num(centre_z)),
        },
        "ShapeLocation": {"ShapePrimitive": "Box", "BoxSize": v3(box)},
        "AddVelocity": {"VelocityType": "Linear", "CoordinateSpace": "Local", "LinearVelocityDistribution": velocity},
        "Drag": {"DragDistribution": frange(drag)},
        "GravityForce": {"bModuleEnabled": "False"},
        "CurlNoiseForce": {"bModuleEnabled": "False"},
        "SpriteRotationRate": {"bModuleEnabled": "True", "RotationRateDistribution": frange(*rotation_rate)},
        "ScaleColor": {"bModuleEnabled": "True", "ScaleDistribution": scale_colour},
        "ScaleSpriteSize": {"bModuleEnabled": "True", "ScaleDistribution": uniform_curve(size_curve)},
        "SubUVAnimation": {"bModuleEnabled": "True", "NumFrames": "4", "AnimationMode": "Random", "RandomChangeInterval": "1000"},
    }
    if acceleration is not None:
        modules["AccelerationForce"] = {"bModuleEnabled": "True", "CoordinateSpace": "Local",
                                        "AccelerationDistribution": v3range(acceleration[0], acceleration[1])}
    return {
        "name": name, "from": FOUNTAIN, "emitter": "Fountain",
        "emitter_properties": {
            "EmitterState": "(LoopBehavior=Infinite,LoopDurationMode=Fixed,LoopDuration=%s,bRecalculateDurationEachLoop=True,"
                            "bEnableDistanceCulling=True,bMaxDistanceEnabled=True,MaxDistance=%s,"
                            "MaxDistanceReaction=SleepAndLetParticlesFinish,bResetAgeOnAwaken=True)" % (frange(*loop), num(CULL_DISTANCE)),
            "SpawnInfos": "((Type=Rate,Rate=%s,bEnabled=True))" % frange(rate),
            "FixedBounds": "(Min=%s,Max=%s,IsValid=True)" % (v3(bounds[0]), v3(bounds[1])),
        },
        "modules": modules,
        "renderer": {
            "Material": material + "." + material.rsplit("/", 1)[1],
            "SubImageSize": "(X=2,Y=2)",
            "bSubImageBlend": "False",
            "Alignment": "Unaligned", "FacingMode": "FaceCamera",
            "bCastShadows": "False",
            "SortMode": "ViewDistance" if sort else "None",
        },
    }


def column(material, rate, size, rise, lean, lifetime=(13.0, 18.0)):
    """A smoke column: rises fast, slows (drag), is pushed downwind harder the higher it gets (acceleration), widens and
    thins. Lit orange by its fire for the first tenth of its life."""
    travel = 0.5 * lean[1] * lifetime[1] ** 2 * 0.75 + 600.0
    top = rise[1] / 0.2 + 1200.0
    return layer("Column", material, rate=rate, lifetime=lifetime, size=size,
                 color_low=(0.95, 0.93, 0.90, 1.0), color_high=(0.95, 0.93, 0.90, 1.0),
                 velocity=v3range((-25.0, -25.0, rise[0]), (25.0, 25.0, rise[1])), box=(140.0, 140.0, 60.0), centre_z=120.0,
                 alpha_curve=[(0, 0.0), (0.04, 2.4), (0.3, 1.6), (0.7, 0.65), (1, 0.0)], size_curve=[(0, 1.0), (0.25, 2.3), (1, 4.6)],
                 bounds=((-600.0 - 0.3 * travel, -600.0, -100.0), (600.0 + 0.3 * travel, 600.0 + travel, top)),
                 drag=0.2, acceleration=((WIND[0] * lean[0], WIND[1] * lean[0], 0.0), (WIND[0] * lean[1], WIND[1] * lean[1], 0.0)),
                 rotation_rate=(-9, 9),
                 colour_curves=([(0, 5.0), (0.06, 1.7), (0.13, 1.0), (1, 1.0)], [(0, 2.0), (0.06, 1.2), (0.13, 1.0), (1, 1.0)],
                                [(0, 0.8), (0.06, 0.9), (0.13, 1.0), (1, 1.0)]),
                 sort=False)


def smoulder(material, rate, lifetime, size, rise, lean, alpha, grow, box):
    """Smoke of one smouldering ruin: slow, large, soft sprites rising, pushed downwind harder the higher they get,
    widening and thinning. alpha is the peak particle alpha (the puff texture and the knob multiply it)."""
    travel = 0.5 * lean * lifetime[1] ** 2 * 0.8 + box
    top = rise[1] / 0.16 + 800.0
    return layer("Smoke", material, rate=rate, lifetime=lifetime, size=size,
                 color_low=(0.95, 0.94, 0.92, 1.0), color_high=(0.95, 0.94, 0.92, 1.0),
                 velocity=v3range((-18.0, -18.0, rise[0]), (18.0, 18.0, rise[1])), box=(box, box, 40.0), centre_z=60.0,
                 alpha_curve=[(0, 0.0), (0.1, alpha), (0.45, alpha * 0.6), (1, 0.0)], size_curve=[(0, 1.0), (0.3, 1.0 + (grow - 1.0) * 0.5), (1, grow)],
                 bounds=((-box - 0.3 * travel, -box - 200.0, -100.0), (box + 0.3 * travel, box + travel, top)),
                 drag=0.16, acceleration=((WIND[0] * lean * 0.8, WIND[1] * lean * 0.8, 0.0), (WIND[0] * lean, WIND[1] * lean, 0.0)),
                 rotation_rate=(-7, 7))


def write_recipe(only=None):
    ruin = FOLDER + "/M_BerlinAtmo_Smoulder"
    mitte = FOLDER + "/M_BerlinAtmo_MitteSmoke"
    vapour = FOLDER + "/M_BerlinAtmo_CraterVapour"
    east = FOLDER + "/M_BerlinAtmo_EastColumn"
    base = FOUNTAIN.split(".")[0]
    systems = [
        # Smouldering ruins, three size classes (alive at once: about 4, 10 and 19 sprites)
        {"target": FOLDER + "/NS_BerlinAtmo_Smoulder_Wisp", "base": base, "disable": ["Fountain"], "stateless": [
            smoulder(ruin, rate=0.45, lifetime=(7, 10), size=(150, 250), rise=(90, 150), lean=14, alpha=1.5, grow=3.0, box=120)]},
        {"target": FOLDER + "/NS_BerlinAtmo_Smoulder_Shell", "base": base, "disable": ["Fountain"], "stateless": [
            smoulder(ruin, rate=0.8, lifetime=(10, 14), size=(260, 400), rise=(130, 210), lean=18, alpha=1.9, grow=3.4, box=260)]},
        {"target": FOLDER + "/NS_BerlinAtmo_Smoulder_Hero", "base": base, "disable": ["Fountain"], "stateless": [
            smoulder(ruin, rate=1.2, lifetime=(14, 18), size=(360, 540), rise=(200, 300), lean=22, alpha=2.4, grow=3.8, box=420)]},
        # Mitte: slow smoke banks in a 50 m box, and paler, lower, quicker dust. About 9 + 6 sprites alive per system.
        {"target": FOLDER + "/NS_BerlinAtmo_MitteSmoke", "base": base, "disable": ["Fountain"], "stateless": [
            layer("Bank", mitte, rate=0.30, lifetime=(24, 34), size=(1100, 1900), color_low=(0.72, 0.71, 0.70, 1), color_high=(0.72, 0.71, 0.70, 1),
                  velocity=wind_velocity(105, 185, 14, 0, 7), box=(5000, 5000, 300), centre_z=330,
                  alpha_curve=[(0, 0.0), (0.18, 0.2), (0.7, 0.14), (1, 0.0)], size_curve=[(0, 0.7), (1, 1.45)],
                  bounds=((-4200, -3400, -200), (4200, 9800, 1900)), rotation_rate=(-3, 3)),
            layer("Dust", mitte, rate=0.5, lifetime=(10, 16), size=(480, 900), color_low=(1.12, 1.05, 0.93, 1), color_high=(1.12, 1.05, 0.93, 1),
                  velocity=wind_velocity(160, 270, 22, 0, 10), box=(5000, 5000, 160), centre_z=140,
                  alpha_curve=[(0, 0.0), (0.2, 0.15), (0.65, 0.10), (1, 0.0)], size_curve=[(0, 0.6), (1, 1.5)],
                  bounds=((-3600, -3000, -200), (3600, 7400, 1100)), rotation_rate=(-7, 7)),
        ]},
        # Crater: a little green vapour over the glass (the heat shimmer is cards, see build_shimmer_material)
        {"target": FOLDER + "/NS_BerlinAtmo_CraterVapour", "base": base, "disable": ["Fountain"], "stateless": [
            layer("Vapour", vapour, rate=0.7, lifetime=(9, 14), size=(650, 1200), color_low=(0.9, 0.95, 0.9, 1), color_high=(0.9, 0.95, 0.9, 1),
                  velocity=v3range((-12, 15, 12), (22, 70, 45)), box=(5600, 5600, 120), centre_z=110,
                  alpha_curve=[(0, 0.0), (0.25, 0.11), (0.7, 0.07), (1, 0.0)], size_curve=[(0, 0.7), (1, 1.5)],
                  bounds=((-3600, -3600, -100), (3600, 4600, 1500)), rotation_rate=(-4, 4)),
        ]},
        # East: the project's own fires, copied, plus a leaning smoke column each
        {"target": FOLDER + "/NS_BerlinAtmo_Fire_Ruin", "base": FIRE_RUIN_BASE, "disable": ["Smoke_"], "stateless": [
            column(east, rate=3.0, size=(250, 360), rise=(380, 520), lean=(24, 34))]},
        {"target": FOLDER + "/NS_BerlinAtmo_Fire_Wreck", "base": FIRE_WRECK_BASE, "disable": ["Smoke_"], "stateless": [
            column(east, rate=2.3, size=(180, 260), rise=(300, 420), lean=(24, 34), lifetime=(11.0, 15.0))]},
    ]
    if only:
        systems = [entry for entry in systems if entry["target"].rsplit("/", 1)[1] in only]
    with open(RECIPE_FILE, "w") as handle:
        json.dump({"systems": systems}, handle, indent=1)
    return RECIPE_FILE, [s["target"] for s in systems]


def build_systems(effect_type, only=None):
    recipe, targets = write_recipe(only)
    unreal.SystemLibrary.execute_console_command(None, "RTS.ImpactVfx.BuildFromRecipe " + recipe)
    for target in targets:
        system = unreal.load_asset(target)
        if system is None:
            log("SYSTEM NOT BUILT " + target)
            continue
        try:
            system.set_editor_property("effect_type", effect_type)
        except Exception as error:  # noqa: BLE001
            log("EFFECT TYPE NOT SET ON %s: %s" % (target, error))
        save(system)


def run(materials=True, systems=True, reset_knobs=False, only=None):
    """only: a set of asset groups to build instead of everything, of "shimmer", "flame" and Niagara system names. For
    when an editor holds the other assets' files (it has the map open): build just what is new, headless."""
    data = placement()
    collection = build_collection(reset_knobs)
    if only is not None:
        if "shimmer" in only:
            build_shimmer_material(collection)
        if "flame" in only:
            build_fire_light_instances(build_light_function("M_LF_BerlinAtmo_Flame", collection, "EastFire", FIRE_LF_CODE, True))
        names = [name for name in only if name.startswith("NS_")]
        if names:
            build_systems(unreal.load_asset(EFFECT_TYPE), names)
        log("DONE (only %s)" % sorted(only))
        return
    if materials:
        mask = import_mask(data["mask"]["texture"])
        build_sheet_material(collection, mask, data["mask"]["rect"])
        build_smoulder_material(collection, mask, data["mask"]["rect"])
        build_puff_material("M_BerlinAtmo_MitteSmoke", collection, "MitteSmoke", "MitteColor", 220.0)
        build_puff_material("M_BerlinAtmo_CraterVapour", collection, "CraterGlow", "CraterColor", 160.0, boost=CRATER_GLOW_BOOST)
        build_puff_material("M_BerlinAtmo_EastColumn", collection, "EastSmoke", "EastSmokeColor", 150.0, camera_fade=(500.0, 1900.0))
        build_shimmer_material(collection)
        fire = build_light_function("M_LF_BerlinAtmo_Flame", collection, "EastFire", FIRE_LF_CODE, True)
        build_fire_light_instances(fire)
        build_light_function("M_LF_BerlinAtmo_Crater", collection, "CraterGlow", CRATER_LF_CODE, False)
    if systems:
        build_systems(build_effect_type())
    log("DONE")


if __name__ == "__main__":
    import sys
    only = [a[5:].split(",") for a in sys.argv if a.startswith("only=")]
    run(reset_knobs="reset" in sys.argv, only=set(only[0]) if only else None)        # run_headless.sh "<this file> reset" puts the knobs back to the defaults above
