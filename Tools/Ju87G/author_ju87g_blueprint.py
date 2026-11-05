"""Ju 87 G (cannon Stuka) assets, step 2 of 2: Stuka mesh copy with pod sockets + the Ju 87 G Blueprint.

Needs a DebugGame build with UAircraftCannonPodsComponent / UAircraftCannonPodsEditorLibrary
(Source/RTS_Survival/Units/Aircraft/CannonPods), so run it headless on that DLL (hold the build lock):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
(or through run_ju87g_headless.py). Step 1 (import_ju87g_assets.py) made the pod mesh and materials.
Idempotent: re-running re-applies every value below. Never touches the original Stuka assets: the sockets go on
SK_Stuka_VTO_Ju87G (a copy), and Ger_Aircraft_StukaJu87G_Airborne is a child of Ger_Aircraft_StukaJu87_Airborne.
Saving the new Blueprint from the shared editor before its Development DLL is rebuilt drops the component.
"""
import unreal

LOG = "[Ju87G] "
FOLDER = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Ju87G"
SOURCE_SK = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"
COPY_SK = FOLDER + "/SK_Stuka_VTO_Ju87G"
POD_MESH = FOLDER + "/SM_Ju87G_BK37Pod"
PARENT_BP = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Ger_Aircraft_StukaJu87_Airborne"
BP_NAME = "Ger_Aircraft_StukaJu87G_Airborne"
BP_PATH = FOLDER + "/" + BP_NAME
COMPONENT_NAME = "CannonPods"

# The Stuka's bones carry a x100 import scale: offsets on stuka_base are in metres (W1, the wing MG, sits at
# 1.40, 2.41, 1.79 = 140, 241, 179 cm). Matches build_bk37_pod.py: pylon top at (0, +-245, 140) cm, the flash hider
# tip 270 cm ahead of it and 36 cm lower. +Y is the right wing.
SOCKET_BONE = "stuka_base"
SOCKETS = {
    "BK37_Pod_L": (0.0, -2.45, 1.40),
    "BK37_Pod_R": (0.0, 2.45, 1.40),
    "BK37_Muzzle_L": (2.70, -2.45, 1.04),
    "BK37_Muzzle_R": (2.70, 2.45, 1.04),
}
PODS = (("BK37_Pod_L", "BK37_Muzzle_L"), ("BK37_Pod_R", "BK37_Muzzle_R"))

# The BK 3,7 was the Flak 18/36 gun in a pod: the cannon reuses the Flak 36 weapon data and fires its APCR
# (tungsten core) shell; the component scales its range to the aircraft gun range so shells reach from the dive.
CANNON = {
    "weapon_name": "Flak36_37MM",
    "fired_shell": "SHELL_APCR",
    "range": 8000.0,
}
VFX_PATHS = {
    "launch_sound": "/Game/RTS_Survival/Sounds/WeaponSounds/MediumCaliber/37mmFlak_Recorded/S_FireWeapon_Flak37_Recorded",
    "launch_sound_fallback": "/Game/RTS_Survival/Sounds/WeaponSounds/MediumCaliber/50mmBk3xBurst/Bk50mm3xBurst_Cue",
    "bounce_sound": "/Game/RTS_Survival/ImpactSfxVfx/1SmallCalibre/Bounce/Cue_SmallCalibre_Bounce",
    "bounce_effect": "/Game/RTS_Survival/ImpactSfxVfx/2MediumCalibre/Bounce/NS_MediumCalibreBounce",
    "launch_effect_fallback": "/Game/RTS_Survival/Blueprints/ProjectilesAndShells/FireWeaponVfxBase/NS_MuzzleFlash_APC_1",
    "impact_attenuation": "/Game/RTS_Survival/Sounds/Settings/MediumCaliber/Att_MediumCaliber_WeaponImpact",
    "impact_concurrency": "/Game/RTS_Survival/Sounds/Settings/MediumCaliber/SC_MediumCaliber_Impact",
    "launch_attenuation": "/Game/RTS_Survival/Sounds/Settings/MediumCaliber/Att_MediumCaliber_Launch",
    "launch_concurrency": "/Game/RTS_Survival/Sounds/Settings/MediumCaliber/SC_MediumCaliber_Launch",
}
MEDIUM = "/Game/RTS_Survival/ImpactSfxVfx/2MediumCalibre/"
IMPACTS = {  # surface: (niagara, cue), as the Me 410's BK 5 (medium calibre impacts)
    "Sand": (MEDIUM + "Sand/NS_MediumCaliber_SandImpact", MEDIUM + "Sand/Cue_MediumCaliber_SandImpact"),
    "Metal": (MEDIUM + "Air_Default/NS_MediumCaliber_ImpactDefault", MEDIUM + "Metal/Cue_MediumCaliber_MetalImapact"),
    "Flesh": (MEDIUM + "Flesh/NS_MediumCaliber_ImpactFlesh", MEDIUM + "Sand/Cue_MediumCaliber_SandImpact"),
    "Air": (MEDIUM + "Air_Default/NS_MediumCaliber_ImpactDefault", MEDIUM + "Sand/Cue_MediumCaliber_SandImpact"),
    "Stone": (MEDIUM + "Stone/NS_MediumCaliber_StoneHit", MEDIUM + "Stone/Cue_MediumCaliber_StoneImpact"),
}
IMPACT_HELPERS = "/Game/RTS_Survival/ImpactSfxVfx/BPF_ImpactHelpers.BPF_ImpactHelpers_C"
LAUNCH_SCALE = 0.5

report = []


def log(line):
    report.append(line)
    unreal.log(LOG + line)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("missing asset " + path)
    return asset


def enum_member(enum_cls, name):
    wanted = name.replace("_", "").lower()
    for member_name in dir(enum_cls):
        if member_name.replace("_", "").lower() == wanted:
            return getattr(enum_cls, member_name)
    raise KeyError("%s has no %s" % (enum_cls.__name__, name))


def ensure_sk_copy():
    sk = unreal.load_asset(COPY_SK)
    if sk is None:
        sk = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_SK, COPY_SK)
        log("duplicated %s -> %s" % (SOURCE_SK, COPY_SK))
    if sk is None:
        raise RuntimeError("could not copy " + SOURCE_SK)
    for name, (x, y, z) in SOCKETS.items():
        ok = unreal.AircraftCannonPodsEditorLibrary.set_skeletal_mesh_socket(
            sk, name, SOCKET_BONE, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, 0.0))
        if not ok:
            raise RuntimeError("socket %s not set" % name)
    names = [str(sk.get_socket_by_index(i).get_editor_property("socket_name")) for i in range(sk.num_sockets())]
    log("sk sockets %s" % names)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sk, False):
        raise RuntimeError("could not save " + COPY_SK)
    log("source mesh sockets (unchanged): %d" % load(SOURCE_SK).num_sockets())
    return sk


def ensure_blueprint():
    bp = unreal.load_asset(BP_PATH)
    if bp is not None:
        return bp
    parent = unreal.load_class(None, PARENT_BP + "." + PARENT_BP.split("/")[-1] + "_C")
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, FOLDER, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError("could not create " + BP_PATH)
    log("created %s (parent %s)" % (BP_PATH, parent.get_name()))
    return bp


def subobjects(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    found = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = library.get_data(handle)
        obj = library.get_object_for_blueprint(data, bp)
        found.append((handle, data, obj))
    return subsystem, found


def find_template(bp, name_part, cls):
    _, found = subobjects(bp)
    for _, _, obj in found:
        if obj is not None and isinstance(obj, cls) and name_part in obj.get_name():
            return obj
    return None


def override_aircraft_mesh(bp, sk):
    mesh = find_template(bp, "AircraftMesh", unreal.SkeletalMeshComponent)
    if mesh is None:
        raise RuntimeError("inherited AircraftMesh component not found")
    mesh.set_editor_property("skeletal_mesh_asset", sk)
    log("AircraftMesh template %s -> %s" % (mesh.get_path_name(), mesh.get_editor_property("skeletal_mesh_asset")))


def ensure_pods_component(bp):
    existing = find_template(bp, COMPONENT_NAME, unreal.AircraftCannonPodsComponent)
    if existing is not None:
        return existing
    subsystem, found = subobjects(bp)
    root_handle = found[0][0]
    params = unreal.AddNewSubobjectParams(parent_handle=root_handle, new_class=unreal.AircraftCannonPodsComponent,
                                          blueprint_context=bp)
    handle, fail_reason = subsystem.add_new_subobject(params)
    if not unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle):
        raise RuntimeError("add component failed: %s" % fail_reason)
    subsystem.rename_subobject(handle, unreal.Text(COMPONENT_NAME))
    component = find_template(bp, COMPONENT_NAME, unreal.AircraftCannonPodsComponent)
    if component is None:
        component = find_template(bp, "", unreal.AircraftCannonPodsComponent)
    log("added component %s" % (component.get_name() if component else None))
    return component


def launch_effect_settings():
    """The cannon muzzle flash as the BK 5 uses it (BPF_ImpactHelpers.GetTankLaunchCannonNoGroundSmoke)."""
    try:
        helpers = unreal.get_default_object(unreal.load_class(None, IMPACT_HELPERS))
        result = helpers.call_method("GetTankLaunchCannonNoGroundSmoke")
        settings = result[0] if isinstance(result, tuple) else result
        if isinstance(settings, unreal.LaunchEffectSettings) and settings.get_editor_property("launch_effect"):
            log("launch effect from BPF_ImpactHelpers: %s" % settings.export_text())
            return settings
        log("BPF_ImpactHelpers returned %s; using the fallback flash" % (result,))
    except Exception as error:
        log("BPF_ImpactHelpers call failed (%s); using the fallback flash" % error)
    settings = unreal.LaunchEffectSettings()
    settings.set_editor_property("launch_effect", load(VFX_PATHS["launch_effect_fallback"]))
    settings.set_editor_property("use_muzzle_launch_smoke", True)
    settings.set_editor_property("use_ground_launch_smoke", False)
    return settings


def build_vfx():
    vfx = unreal.WeaponVFX()
    sound = unreal.load_asset(VFX_PATHS["launch_sound"])
    if not isinstance(sound, unreal.SoundCue):
        log("launch sound %s is %s, using the BK 5 cue" % (VFX_PATHS["launch_sound"], type(sound).__name__))
        sound = load(VFX_PATHS["launch_sound_fallback"])
    vfx.set_editor_property("launch_sound", sound)
    vfx.set_editor_property("bounce_sound", load(VFX_PATHS["bounce_sound"]))
    vfx.set_editor_property("bounce_effect", load(VFX_PATHS["bounce_effect"]))
    vfx.set_editor_property("launch_effect_settings", launch_effect_settings())
    impacts = {}
    for surface, (effect, sound_path) in IMPACTS.items():
        data = unreal.RTSSurfaceImpactData()
        data.set_editor_property("impact_effect", load(effect))
        data.set_editor_property("impact_sound", load(sound_path))
        impacts[enum_member(unreal.RTSSurfaceType, surface)] = data
    vfx.set_editor_property("surface_impact_effects", impacts)
    vfx.set_editor_property("launch_scale", unreal.Vector(LAUNCH_SCALE, LAUNCH_SCALE, LAUNCH_SCALE))
    vfx.set_editor_property("impact_scale", unreal.Vector(1.0, 1.0, 1.0))
    vfx.set_editor_property("bounce_scale", unreal.Vector(1.0, 1.0, 1.0))
    for key in ("impact_attenuation", "impact_concurrency", "launch_attenuation", "launch_concurrency"):
        vfx.set_editor_property(key, load(VFX_PATHS[key]))
    return vfx


def build_cannon_weapon():
    weapon = unreal.InitWeaponStateProjectile()
    weapon.set_editor_property("weapon_name", CANNON["weapon_name"])
    weapon.set_editor_property("weapon_burst_mode", enum_member(unreal.WeaponFireMode, "Single"))
    weapon.set_editor_property("projectile_system", enum_member(unreal.ProjectileNiagaraSystem, "TankShell"))
    weapon.set_editor_property("weapon_vfx", build_vfx())
    weapon.set_editor_property("burst_cooldown", 0.0)
    weapon.set_editor_property("single_burst_amount_max_burst_amount", 1)
    weapon.set_editor_property("min_burst_amount", 1)
    return weapon


def configure_component(component, pod_mesh):
    pods = []
    for attach, muzzle in PODS:
        pod = unreal.AircraftCannonPod()
        pod.set_editor_property("pod_mesh", pod_mesh)
        pod.set_editor_property("attach_socket_name", attach)
        pod.set_editor_property("muzzle_socket_name", muzzle)
        pods.append(pod)
    component.set_editor_property("pods", pods)
    component.set_editor_property("cannon_weapon", build_cannon_weapon())
    component.set_editor_property("fired_shell_type", enum_member(unreal.WeaponShellType, CANNON["fired_shell"]))
    component.set_editor_property("cannon_range", CANNON["range"])
    component.set_editor_property("remove_bomb_bay", True)
    component.set_editor_property("target_preference", enum_member(unreal.TargetPreference, "Tank"))
    log("component pods=%d weapon=%s shell=%s range=%s" % (
        len(component.get_editor_property("pods")),
        component.get_editor_property("cannon_weapon").get_editor_property("weapon_name"),
        component.get_editor_property("fired_shell_type"), component.get_editor_property("cannon_range")))
    log("cannon weapon text %s" % component.get_editor_property("cannon_weapon").export_text()[:1500])


def run():
    sk = ensure_sk_copy()
    pod_mesh = load(POD_MESH)
    bp = ensure_blueprint()
    override_aircraft_mesh(bp, sk)
    component = ensure_pods_component(bp)
    configure_component(component, pod_mesh)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    log("saved %s=%s" % (BP_PATH, saved))
    # Verify through the generated class: the CDO's components come from the templates just saved.
    generated = unreal.load_class(None, BP_PATH + "." + BP_NAME + "_C")
    log("generated class %s parent %s" % (generated.get_name() if generated else None,
                                           unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_name()
                                           if hasattr(unreal.BlueprintEditorLibrary, "get_blueprint_parent_class")
                                           else "?"))
    check = find_template(bp, "AircraftMesh", unreal.SkeletalMeshComponent)
    log("verify AircraftMesh asset %s" % check.get_editor_property("skeletal_mesh_asset").get_name())


try:
    run()
except Exception as error:
    import traceback
    log("ERROR %s %s" % (error, traceback.format_exc().replace("\n", " | ")))
