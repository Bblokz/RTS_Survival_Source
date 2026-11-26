"""Gives the V2 rocket (GA_GerV2Rocket) its crater and the bigger detonation.

Step 1 builds two material instances and needs no new C++:
  MI_V2RocketCraterPit  the parallax crater decal: a child of the pack's MI_Decal_Crater_Scorched_Dirt_04 (its
                        textures on M_Decal_Parallax), with deeper parallax.
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
# Setting the four textures on a direct child of M_Decal_Parallax did not stick (the pit drew black), so the pit
# inherits them from the pack's own instance of texture 04.
PIT_PARENT = "/Game/Crater_Decals/Decals/Scorched/MI_Decal_Crater_Scorched_Dirt_04"
RIM_PARENT = "/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Materials/M_FieldTrench_Bank"
PIT_TEXTURE = "/Game/Crater_Decals/Textures/T_crater_scorched_dirt_04_basecolor_alpha"

# Texture 04: round bowl, its rim crest at about 0.6 of the half size (radial mean of the height map: 0.89 at 0.55-0.65).
# The pack's own decals use 0.11-0.125 at a few metres; at 60 m a deeper bowl reads from the RTS camera.
PIT_SCALARS = {"Parallax_Depth": 0.16, "Parallax_MaxSteps": 48.0, "Normal_Intensity": 1.4, "AO Strength": 1.0,
               "Extra Fake AO Strength": 0.6}
RIM_SCALARS = {"FreshStrength": 0.85, "FreshSoilAmount": 0.75}
# Burnt, freshly thrown earth on the crest. 0.035 drew the rim as a black ring from the RTS camera.
RIM_VECTORS = {"FreshSoilColor": (0.1, 0.075, 0.055, 1.0)}

# Rim crest at the core radius (18 m): a crater 36 m across, its decal 60 m. The blast still reaches 45 m.
CRATER = ("(RimRadius=1800.000000,PitDecalMaterial=\"{pit}\",DecalRimFraction=0.600000,RimMaterial=\"{rim}\","
          "RimHeight=180.000000,RimInnerRunFraction=0.380000,RimOuterRunFraction=0.600000,RimIrregularity=0.150000,"
          "bRimCarriesVehicles=True)")


def path(asset):
    return "%s.%s" % (asset, asset.split("/")[-1])


# (effect, count, scale, min delay, max delay, min radius, max radius); radius as part of the 45 m blast radius.
LAYERS = [
    # Rolling ring of smoke and big stones thrown out of the pit.
    # At 6x its translucent sprites were tens of metres and cut flat into the ground.
    ("/Game/RTS_Survival/ImpactSfxVfx/4SuperHeavyCalibre/Stone/NS_RTSDebrisExplosion_Huge", 1, 3.5, 0.0, 0.0, 0.0, 0.0),
    # A fiery ground burst under the mushroom.
    ("/Game/BigExplosions/Niagara/NS_Ground_2", 1, 1.6, 0.0, 0.0, 0.0, 0.0),
    # Earth thrown up all along the rim.
    ("/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Howitzer/NS_HowitzerImpact_Regular", 10, 2.5, 0.05, 0.45, 0.32, 0.48),
    # Fuel and ammunition going up around the blast after the fireball.
    ("/Game/RTS_Survival/SpecialEffects/AA_OptimExplosions/NS_RTS_Explosion_Big_9", 6, 2.0, 0.3, 1.8, 0.2, 0.75),
    # No NS_DestructionSmoke_Large: its dithered building smoke went black and hid the crater for over 10 s.
]
FLASH = "FlashIntensity=90000000.000000,FlashFadeSeconds=3.500000"
SHAKE = "BlastShakePulses=4,BlastShakePulseIntervalSeconds=0.400000"

# (sound, delay s, volume, pitch). A V2 outran its own sound: siren at launch, silence, the blast, then the roar of the
# rocket arriving. The fly-by peaks 1.5 s into the file and fades over ten more; pitched down it reads as a heavy rocket.
AFTER_BLAST_SOUNDS = [
    ("/Game/WARFARE_SOUNDS/Miscellaneous_Sounds/Cinematic_Sounds/Cues/cinematic_deep_bass_rumble_01_Cue", 0.0, 1.0, 1.0),
    ("/Game/Aircraft_Sound_Vol1/Aircraft_Sound_Effects/Aircraft_Jet_Effects/Cue/Sus4_F18Fly_By_Fast_1_Cue", 1.1, 1.0,
     0.6),
]
# Hit-stop and screen flash, only for a blast on screen.
IMPACT_FEEL = ("ImpactFeel=(HitStopSeconds=0.130000,HitStopTimeDilation=0.080000,ScreenFlashSeconds=0.700000,"
               "ScreenFlashExposureBias=3.500000,ScreenFlashBloomIntensity=4.000000,ScreenFringeIntensity=5.000000,"
               "MaxCameraDistance=30000.000000)")
STONES = "/Game/RTS_Survival/Blueprints/Environment/GroundStones/Meshes/SM_GS_"
DEBRIS_MESHES = [STONES + name for name in ("Chunk_A", "Chunk_B", "Chunk_C", "Chunk_D", "Slab_A", "Slab_B")]
DEBRIS_LANDING_EFFECT = "/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Stone/NS_Artillery_StoneImpact"
DEBRIS_LANDING_SOUND = "/Game/RTS_Survival/ImpactSfxVfx/1SmallCalibre/Stone/Cue_SmallCaliber_StoneImpact"
# Thrown up through the fireball (a 4.5 s flight peaks about 25 m up), landing around the crater rim and beyond.
DEBRIS = ("DebrisRain=(Count=40,Meshes=({meshes}),MinScale=0.500000,MaxScale=1.500000,MinFlightSeconds=1.800000,"
          "MaxFlightSeconds=4.800000,MinRangeFraction=0.650000,MaxRangeFraction=1.250000,"
          "LandingEffect=\"{effect}\",LandingEffectScale=0.450000,LandingSound=\"{sound}\",MaxLandingSounds=14)")


def layers_text():
    entries = []
    for effect, count, scale, min_delay, max_delay, min_radius, max_radius in LAYERS:
        entries.append("(Effect=\"%s\",Count=%d,Scale=%f,MinDelaySeconds=%f,MaxDelaySeconds=%f,"
                       "MinRadiusFraction=%f,MaxRadiusFraction=%f)"
                       % (path(effect), count, scale, min_delay, max_delay, min_radius, max_radius))
    return "DetonationLayers=(%s)" % ",".join(entries)


# Soldiers' bodies fly 1.8x as fast; killed tanks blow their turrets off away from the blast; survivors are shoved
# up to 9 m/s outward and 3.5 m/s up at ground zero (falling off to the edge) with their tracks let go for 1.3 s.
BLAST_THROW = ("BlastThrow=(InfantryThrowScale=1.800000,bTossTurrets=True,TurretTossSpeedScale=1.400000,"
               "VehiclePushSpeed=900.000000,VehicleLiftSpeed=350.000000,VehicleSpinDegrees=40.000000,"
               "VehicleKnockbackSeconds=1.300000)")


def after_blast_sounds_text():
    entries = ["(Sound=\"%s\",DelaySeconds=%f,VolumeMultiplier=%f,PitchMultiplier=%f)"
               % (path(sound), delay, volume, pitch) for sound, delay, volume, pitch in AFTER_BLAST_SOUNDS]
    return "AfterBlastSounds=(%s)" % ",".join(entries)


def debris_text():
    meshes = ",".join('"%s"' % path(mesh) for mesh in DEBRIS_MESHES)
    return DEBRIS.format(meshes=meshes, effect=path(DEBRIS_LANDING_EFFECT), sound=path(DEBRIS_LANDING_SOUND))


def make_instance(asset_path, parent_path):
    instance = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    if instance is None:
        folder, name = asset_path.rsplit("/", 1)
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, unreal.load_asset(parent_path))
    # Without this the parameter setters cannot see the new parent's parameters and silently do nothing.
    unreal.MaterialEditingLibrary.update_material_instance(instance)
    return instance


def build_materials():
    report = []
    lib = unreal.MaterialEditingLibrary
    pit = make_instance(PIT_MI, PIT_PARENT)
    for parameter, value in PIT_SCALARS.items():
        lib.set_material_instance_scalar_parameter_value(pit, parameter, value)
    lib.update_material_instance(pit)
    # The setters' return values are not reliable; read every value back.
    for parameter, value in PIT_SCALARS.items():
        if abs(lib.get_material_instance_scalar_parameter_value(pit, parameter) - value) > 1e-3:
            return ["ERROR pit scalar %s not set" % parameter], False
    texture = lib.get_material_instance_texture_parameter_value(pit, "BaseColour_Opacity")
    if texture is None or texture.get_path_name().split(".")[0] != PIT_TEXTURE:
        return ["ERROR pit BaseColour_Opacity is %s" % (texture.get_path_name() if texture else None)], False
    rim = make_instance(RIM_MI, RIM_PARENT)
    for parameter, value in RIM_SCALARS.items():
        lib.set_material_instance_scalar_parameter_value(rim, parameter, value)
    for parameter, value in RIM_VECTORS.items():
        lib.set_material_instance_vector_parameter_value(rim, parameter, unreal.LinearColor(*value))
    lib.update_material_instance(rim)
    for parameter, value in RIM_SCALARS.items():
        if abs(lib.get_material_instance_scalar_parameter_value(rim, parameter) - value) > 1e-3:
            return ["ERROR rim scalar %s not set" % parameter], False
    for parameter, value in RIM_VECTORS.items():
        if abs(lib.get_material_instance_vector_parameter_value(rim, parameter).r - value[0]) > 1e-3:
            return ["ERROR rim vector %s not set" % parameter], False
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
    exported = visuals.export_text()
    if "BlastThrow=" not in exported or "DebrisRain=" not in exported:
        return ["SKIP template visuals lack BlastThrow/DebrisRain: this DLL predates the impact C++"], False
    fresh = type(visuals)()
    fresh.import_text(visuals.export_text())
    crater = CRATER.format(pit=path(PIT_MI), rim=path(RIM_MI))
    text = "(Crater=%s,%s,%s,%s,%s,%s,%s,%s)" % (crater, layers_text(), FLASH, SHAKE, after_blast_sounds_text(),
                                                IMPACT_FEEL, debris_text(), BLAST_THROW)
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
