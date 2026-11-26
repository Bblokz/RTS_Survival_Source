"""Gives the V2 rocket (GA_GerV2Rocket) its crater and the bigger detonation.

Step 1 builds two material instances and needs no new C++:
  MI_V2RocketCraterPit  the parallax crater decal (crater-decal pack texture 04 on M_Decal_Parallax), deeper parallax.
  MI_V2RocketCraterRim  the earth rim (M_FieldTrench_Bank: RVT ground colour, burnt fresh spoil on the crest).
Step 2 writes only the crater, detonation layers, flash and shake fields of the template's m_nuke_visuals; settings,
costs, cooldown and texts stay as setup_v2_rocket.py left them. It needs a DLL whose FRadixiteNukeVisuals has
`Crater` and `DetonationLayers`; on an older DLL it reports that and saves nothing for step 2.

Run in a DebugGame editor or commandlet on the new DLL:
    UnrealEditor-Win64-DebugGame-Cmd RTS_Survival.uproject -run=pythonscript -script=<this file>
Step 1 alone is safe in any editor: set V2_CRATER_STEPS = ("materials",) before exec'ing this file.
"""
import unreal

LOG_PREFIX = "[V2Crater] "
DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
AREAS = "/Game/RTS_Survival/Materials/AimMaterials/TargetAreas/"
PIT_MI = AREAS + "MI_V2RocketCraterPit"
RIM_MI = AREAS + "MI_V2RocketCraterRim"
PIT_PARENT = "/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteDecals/Parent/M_Decal_Parallax"
RIM_PARENT = "/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Materials/M_FieldTrench_Bank"
CRATER_TEXTURES = "/Game/Crater_Decals/Textures/T_crater_scorched_dirt_04_"

# Texture 04: round bowl, its rim crest at about 0.6 of the half size (radial mean of the height map: 0.89 at 0.55-0.65).
PIT_TEXTURES = {"BaseColour_Opacity": "basecolor_alpha", "Height": "height", "Normal": "normal", "RMAO": "RMAO"}
# The pack's own decals use 0.11-0.125 at a few metres; at 60 m a deeper bowl reads from the RTS camera.
PIT_SCALARS = {"Parallax_Depth": 0.16, "Parallax_MaxSteps": 48.0, "Normal_Intensity": 1.4, "AO Strength": 1.0,
               "Extra Fake AO Strength": 0.8}
RIM_SCALARS = {"FreshStrength": 1.0, "FreshSoilAmount": 1.0}
# Burnt, freshly thrown earth on the crest.
RIM_VECTORS = {"FreshSoilColor": (0.035, 0.028, 0.022, 1.0)}

# Rim crest at the core radius (18 m): a crater 36 m across, its decal 60 m. The blast still reaches 45 m.
CRATER = ("(RimRadius=1800.000000,PitDecalMaterial=\"{pit}\",DecalRimFraction=0.600000,RimMaterial=\"{rim}\","
          "RimHeight=180.000000,RimInnerRunFraction=0.380000,RimOuterRunFraction=0.600000,RimIrregularity=0.150000,"
          "bRimCarriesVehicles=True)")


def path(asset):
    return "%s.%s" % (asset, asset.split("/")[-1])


# (effect, count, scale, min delay, max delay, min radius, max radius); radius as part of the 45 m blast radius.
LAYERS = [
    # Rolling ring of smoke and big stones thrown out of the pit.
    ("/Game/RTS_Survival/ImpactSfxVfx/4SuperHeavyCalibre/Stone/NS_RTSDebrisExplosion_Huge", 1, 6.0, 0.0, 0.0, 0.0, 0.0),
    # A fiery ground burst under the mushroom.
    ("/Game/BigExplosions/Niagara/NS_Ground_2", 1, 1.6, 0.0, 0.0, 0.0, 0.0),
    # Earth thrown up all along the rim.
    ("/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Howitzer/NS_HowitzerImpact_Regular", 10, 2.5, 0.05, 0.45, 0.32, 0.48),
    # Fuel and ammunition going up around the blast after the fireball.
    ("/Game/RTS_Survival/SpecialEffects/AA_OptimExplosions/NS_RTS_Explosion_Big_9", 6, 2.0, 0.3, 1.8, 0.2, 0.75),
    # Smoke rising out of the crater while the dust settles.
    ("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke_Large", 4, 3.0,
     1.2, 3.0, 0.0, 0.25),
]
FLASH = "FlashIntensity=90000000.000000,FlashFadeSeconds=3.500000"
SHAKE = "BlastShakePulses=4,BlastShakePulseIntervalSeconds=0.400000"


def layers_text():
    entries = []
    for effect, count, scale, min_delay, max_delay, min_radius, max_radius in LAYERS:
        entries.append("(Effect=\"%s\",Count=%d,Scale=%f,MinDelaySeconds=%f,MaxDelaySeconds=%f,"
                       "MinRadiusFraction=%f,MaxRadiusFraction=%f)"
                       % (path(effect), count, scale, min_delay, max_delay, min_radius, max_radius))
    return "DetonationLayers=(%s)" % ",".join(entries)


def make_instance(asset_path, parent_path):
    instance = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    if instance is None:
        folder, name = asset_path.rsplit("/", 1)
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, unreal.load_asset(parent_path))
    return instance


def build_materials():
    report = []
    lib = unreal.MaterialEditingLibrary
    pit = make_instance(PIT_MI, PIT_PARENT)
    for parameter, suffix in PIT_TEXTURES.items():
        texture = unreal.load_asset(CRATER_TEXTURES + suffix)
        if texture is None:
            return ["ERROR texture %s%s missing" % (CRATER_TEXTURES, suffix)], False
        lib.set_material_instance_texture_parameter_value(pit, parameter, texture)
    for parameter, value in PIT_SCALARS.items():
        lib.set_material_instance_scalar_parameter_value(pit, parameter, value)
    lib.update_material_instance(pit)
    rim = make_instance(RIM_MI, RIM_PARENT)
    for parameter, value in RIM_SCALARS.items():
        lib.set_material_instance_scalar_parameter_value(rim, parameter, value)
    for parameter, value in RIM_VECTORS.items():
        lib.set_material_instance_vector_parameter_value(rim, parameter, unreal.LinearColor(*value))
    lib.update_material_instance(rim)
    saved = all(unreal.EditorAssetLibrary.save_loaded_asset(asset, False) for asset in (pit, rim))
    report.append("materials saved=%s" % saved)
    return report, saved


def find_v2_template(asset):
    templates = asset.get_editor_property("m_ability_templates_by_type")
    for key, template in templates.items():
        if "V2_ROCKET" in str(key).upper().replace("V_2", "V2"):
            return template
    return None


def apply_visuals():
    asset = unreal.load_asset(DA_PATH)
    template = find_v2_template(asset)
    if template is None:
        return ["ERROR no V2 template in %s" % DA_PATH], False
    visuals = template.get_editor_property("m_nuke_visuals")
    if "Crater=" not in visuals.export_text():
        return ["SKIP template visuals have no Crater field: this DLL predates the crater C++"], False
    fresh = type(visuals)()
    fresh.import_text(visuals.export_text())
    crater = CRATER.format(pit=path(PIT_MI), rim=path(RIM_MI))
    text = "(Crater=%s,%s,%s,%s)" % (crater, layers_text(), FLASH, SHAKE)
    fresh.import_text(text)
    asset.modify()
    template.set_editor_property("m_nuke_visuals", fresh)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    written = find_v2_template(unreal.load_asset(DA_PATH)).get_editor_property("m_nuke_visuals").export_text()
    return ["visuals saved=%s" % saved, "visuals " + written], saved


def run(steps):
    report = []
    if "materials" in steps:
        lines, ok = build_materials()
        report += lines
        if not ok:
            return report + ["NOT continuing"]
    if "visuals" in steps:
        lines, _ = apply_visuals()
        report += lines
    return report


for line in run(globals().get("V2_CRATER_STEPS", ("materials", "visuals"))):
    unreal.log(LOG_PREFIX + line)
    print(LOG_PREFIX + line)
