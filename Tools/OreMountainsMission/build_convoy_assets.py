"""Builds the Ore Mountains convoy assets (re-runnable; deletes and rebuilds its own assets only).

Headless:  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
           -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate
Folder:    /Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/Convoys/

Trucks: ASurrenderingCargoTruck / ASurrenderingCaseTruck do not inherit RTS_BP_TrackedTankMaster, so each gets an
intermediate master that is a copy of RTS_BP_TrackedTankMaster reparented onto the C++ class (keeps the AI controller,
components and graph). The truck Blueprints are copies of the Zil trucks reparented onto that master; the reparent
drops the Zil's overrides of the master's components (their key points at RTS_BP_TrackedTankMaster_C), so they are
copied back from the Zil and read back. Parent calls in the Zil graphs compile against the new parent by name.
"""
import re
import warnings

import unreal

BASE = "/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/Convoys/"
# The map's data assets live next to the level (owner, 2026-11-24).
DATA = "/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains/"
MASTER = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/RTS_BP_TrackedTankMaster"
ZIL = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilCargo/"
ZIL_AMMO = ZIL + "Rus_ArmoredCar_ZilAmmo"
ZIL_FUEL = ZIL + "Rus_ArmoredCar_ZilFuel"
SCAV_PARTS = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts1"
CACHE_RADIXITE = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/Radixite/BP_ScavObj_RadixiteDump.BP_ScavObj_RadixiteDump_C"
CACHE_PARTS = SCAV_PARTS + ".ScavObj_VehicleParts1_C"
FLAG_SRC_MESH = "/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Meshes/SM_SupplyPoint_Flag"
BANNER_MATERIAL = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/Mat/M_SovietBanner"
WHITE_TEXTURE = "/Engine/EngineResources/WhiteSquareTexture"
CASES_MESH = "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/meshes/SM_Airport_Crate_Stack_02"
CONVOY_TEMPLATE = "/Game/RTS_Survival/Blueprints/Convoy/DA_Convoy_Fuel"

EAL = unreal.EditorAssetLibrary
BEL = unreal.BlueprintEditorLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
REPORT = []


def log(text):
    REPORT.append(str(text))
    unreal.log_warning("ORE_CONVOY_ASSETS " + str(text))


# ---- helpers -----------------------------------------------------------------------------------------------------

def fresh_duplicate(src, dst):
    if EAL.does_asset_exist(dst):
        EAL.delete_asset(dst)
    obj = EAL.duplicate_asset(src, dst)
    if obj is None:
        raise RuntimeError("duplicate failed %s -> %s" % (src, dst))
    return obj


def save(path):
    ok = EAL.save_asset(path, only_if_is_dirty=False)
    log("saved %s %s" % (path, ok))
    if not ok:
        raise RuntimeError("save failed " + path)


def gen_class(bp):
    return bp.generated_class()


def cdo(bp):
    return unreal.get_default_object(bp.generated_class())


def components(bp):
    out = {}
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDL.get_data(handle)
        name = str(SDL.get_variable_name(data))
        if name in out or name == "None":
            continue
        out[name] = data
    return out


ADDRESS = re.compile(r"\(0x[0-9A-Fa-f]+\)")


def same_value(a, b):
    try:
        if a == b:
            return True
    except Exception:
        pass
    # struct reprs carry their memory address
    return ADDRESS.sub("", str(a)) == ADDRESS.sub("", str(b))


def copyable(value, src_package):
    if isinstance(value, unreal.ActorComponent):
        return False
    if isinstance(value, unreal.Object):
        try:
            if value.get_outermost() == src_package:
                return False
        except Exception:
            return False
    return True


def property_names(obj):
    names = []
    for name in dir(obj):
        # skip delegates (bound to the source) and actor/instance GUIDs
        if name.startswith("_") or name.startswith("on_") or "guid" in name:
            continue
        with warnings.catch_warnings():
            warnings.simplefilter("error", DeprecationWarning)   # deprecated aliases of other properties
            try:
                obj.get_editor_property(name)
            except Exception:
                continue
        names.append(name)
    return names


def copy_properties(src, dst, src_package):
    """Copies every editor property of src that differs on dst; returns (copied, still differing)."""
    copied, failed = [], []
    for name in property_names(src):
        try:
            a = src.get_editor_property(name)
            b = dst.get_editor_property(name)
        except Exception:
            continue
        if same_value(a, b):
            continue
        if not copyable(a, src_package):
            failed.append(name + "(ref)")
            continue
        try:
            dst.set_editor_property(name, a)
            copied.append(name)
        except Exception as error:
            failed.append("%s(%s)" % (name, str(error)[:60]))
    return copied, failed


def diff_properties(src, dst):
    out = []
    for name in property_names(src):
        try:
            if not same_value(src.get_editor_property(name), dst.get_editor_property(name)):
                out.append(name)
        except Exception:
            continue
    return out


# Properties that read as different through str() but are equal in effect, or are transient/derived.
IGNORED_DIFFS = {"body_instance", "lighting_channels", "relative_translation", "skeletal_mesh", "skinned_asset",
                 "animation_data", "shape_color"}


def restore_component_overrides(src_bp, dst_bp):
    """Copies src_bp's values of the components dst_bp inherits (overrides lost on reparent) and reads them back."""
    src_package = src_bp.get_outermost()
    src_comps = components(src_bp)
    dst_comps = components(dst_bp)
    problems = []
    for name, src_data in src_comps.items():
        if name not in dst_comps:
            problems.append("missing component " + name)
            continue
        src_obj = SDL.get_object_for_blueprint(src_data, src_bp)
        dst_obj = SDL.get_object_for_blueprint(dst_comps[name], dst_bp)
        if src_obj is None or dst_obj is None:
            continue
        copied, failed = copy_properties(src_obj, dst_obj, src_package)
        if copied or failed:
            log("  %s: copied %s %s" % (name, copied, ("FAILED " + str(failed)) if failed else ""))
    # actor defaults (class defaults of the BP)
    copied, failed = copy_properties(cdo(src_bp), cdo(dst_bp), src_package)
    log("  class defaults: copied %s %s" % (copied, ("FAILED " + str(failed)) if failed else ""))
    return problems


def verify_against(src_bp, dst_bp):
    left = {}
    src_comps = components(src_bp)
    dst_comps = components(dst_bp)
    for name, src_data in src_comps.items():
        if name not in dst_comps:
            left[name] = ["MISSING"]
            continue
        src_obj = SDL.get_object_for_blueprint(src_data, src_bp)
        dst_obj = SDL.get_object_for_blueprint(dst_comps[name], dst_bp)
        if src_obj is None or dst_obj is None:
            continue
        diffs = [d for d in diff_properties(src_obj, dst_obj) if d not in IGNORED_DIFFS]
        if diffs:
            left[name] = diffs
    cdo_diffs = [d for d in diff_properties(cdo(src_bp), cdo(dst_bp)) if d not in IGNORED_DIFFS]
    if cdo_diffs:
        left["<class defaults>"] = cdo_diffs
    return left


def compile_bp(bp):
    BEL.compile_blueprint(bp)


# ---- white flag ---------------------------------------------------------------------------------------------------

def build_white_flag():
    mi_path = BASE + "MI_Ore_WhiteFlag"
    if EAL.does_asset_exist(mi_path):
        EAL.delete_asset(mi_path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mi = tools.create_asset("MI_Ore_WhiteFlag", BASE.rstrip("/"), unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, unreal.load_asset(BANNER_MATERIAL))
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "Banner", unreal.load_asset(WHITE_TEXTURE))
    # the same wind as the faction banners, a little calmer
    for name, value in (("WindSpeed", 4.0), ("WindStrength", 0.8), ("AnchorPower", 2.0)):
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, name, value)
    unreal.MaterialEditingLibrary.update_material_instance(mi)
    save(mi_path)
    mesh_path = BASE + "SM_Ore_WhiteFlag"
    mesh = fresh_duplicate(FLAG_SRC_MESH, mesh_path)
    mesh.set_material(0, mi)
    save(mesh_path)
    return mesh


def flag_transform():
    # SM_SupplyPoint_Flag is a flat 160 x 95 cm cloth in XY with its anchor edge on y = 0; roll -90 hangs it down
    # in the XZ plane (seen from the sides), so it hangs from z 390 over the cab roof.
    return unreal.Transform(location=unreal.Vector(140.0, 0.0, 390.0), rotation=unreal.Rotator(roll=-90.0, pitch=0.0, yaw=0.0),
                            scale=unreal.Vector(0.75, 0.75, 0.75))


# ---- sealed cache --------------------------------------------------------------------------------------------------

def build_sealed_cases():
    path = BASE + "BP_Ore_SealedCases"
    bp = fresh_duplicate(SCAV_PARTS, path)
    comps = components(bp)
    mesh_comp = SDL.get_object_for_blueprint(comps["Mesh"], bp)
    mesh_comp.set_editor_property("static_mesh", unreal.load_asset(CASES_MESH))
    widget = SDL.get_object_for_blueprint(comps["ProgressBarWidget"], bp)
    widget.set_editor_property("relative_location", unreal.Vector(20.0, 70.0, 230.0))
    defaults = cdo(bp)
    # starter reward for the Berlin cases (the truck's SealedCache.ScavengeSeconds = 30 sets the time)
    rewards = defaults.get_editor_property("scavenge_resources")
    new_rewards = {}
    for key in rewards.keys():
        entry = rewards[key]
        entry.set_editor_property("is_set_amount", True)
        entry.set_editor_property("set_amount", 150 if key == unreal.RTSResourceType.RESOURCE_VEHICLE_PARTS else 100)
        new_rewards[key] = entry
    defaults.set_editor_property("scavenge_resources", new_rewards)
    defaults.set_editor_property("scavenge_time", 30.0)
    compile_bp(bp)
    save(path)
    return bp


# ---- trucks --------------------------------------------------------------------------------------------------------

def build_master(name, native_class, flag_mesh, sealed_cases_class=None):
    path = BASE + name
    bp = fresh_duplicate(MASTER, path)
    BEL.reparent_blueprint(bp, native_class.static_class())
    compile_bp(bp)
    defaults = cdo(bp)
    display = defaults.get_editor_property("display_settings")
    display.set_editor_property("white_flag_mesh", flag_mesh)
    display.set_editor_property("white_flag_relative_transform", flag_transform())
    defaults.set_editor_property("display_settings", display)
    if sealed_cases_class is not None:
        case_settings = defaults.get_editor_property("case_truck_settings")
        sealed = case_settings.get_editor_property("sealed_cache")
        sealed.set_editor_property("sealed_cache_class", sealed_cases_class)
        sealed.set_editor_property("scavenge_seconds", 30.0)
        case_settings.set_editor_property("sealed_cache", sealed)
        defaults.set_editor_property("case_truck_settings", case_settings)
    compile_bp(bp)
    save(path)
    log("master %s parent=%s ai=%s" % (name, native_class.static_class().get_name(), defaults.get_editor_property("ai_controller_class")))
    return bp


def build_truck(name, zil_path, master_bp, sealed_cases_class=None):
    path = BASE + name
    zil = unreal.load_asset(zil_path)
    bp = fresh_duplicate(zil_path, path)
    BEL.reparent_blueprint(bp, master_bp.generated_class())
    compile_bp(bp)
    log("truck %s from %s onto %s" % (name, zil_path.split("/")[-1], master_bp.get_name()))
    problems = restore_component_overrides(zil, bp)
    for problem in problems:
        log("  PROBLEM " + problem)
    defaults = cdo(bp)
    display = defaults.get_editor_property("display_settings")
    display.set_editor_property("white_flag_mesh", cdo(master_bp).get_editor_property("display_settings").get_editor_property("white_flag_mesh"))
    display.set_editor_property("white_flag_relative_transform", flag_transform())
    defaults.set_editor_property("display_settings", display)
    if sealed_cases_class is not None:
        case_settings = defaults.get_editor_property("case_truck_settings")
        sealed = case_settings.get_editor_property("sealed_cache")
        sealed.set_editor_property("sealed_cache_class", sealed_cases_class)
        sealed.set_editor_property("scavenge_seconds", 30.0)
        case_settings.set_editor_property("sealed_cache", sealed)
        defaults.set_editor_property("case_truck_settings", case_settings)
    compile_bp(bp)
    left = verify_against(zil, bp)
    log("  read back vs %s: %s" % (zil.get_name(), left if left else "identical"))
    save(path)
    return bp


# ---- convoy definitions ---------------------------------------------------------------------------------------------

def tank(subtype):
    return unreal.TrainingOption(unit_type=unreal.AllUnitType.UN_TYPE_TANK, subtype_value=int(subtype.value))


def squad(subtype):
    return unreal.TrainingOption(unit_type=unreal.AllUnitType.UN_TYPE_SQUAD, subtype_value=int(subtype.value))


T = unreal.TankSubtype
S = unreal.SquadSubtype
ST = unreal.ConvoyStation
CARGO = unreal.ConvoyCargoType
RES = unreal.RTSResourceType


def escort(option, station, count=1):
    return unreal.ConvoyEscortSpec(unit=option, count=count, station=station)


def tier(name, extra, escorts):
    return unreal.ConvoyGuardTier(tier_name=name, escorts=escorts, extra_trucks=extra)


def truck_spec(bp, option, count, speed, cargo_override=None):
    spec = unreal.ConvoyTruckSpec()
    spec.set_editor_property("truck_class", bp.generated_class())
    spec.set_editor_property("training_option", option)
    spec.set_editor_property("count", count)
    spec.set_editor_property("calibrated_cruise_speed_cms", speed)
    if cargo_override is not None:
        spec.set_editor_property("override_cargo_type", True)
        spec.set_editor_property("cargo_type_override", cargo_override)
    return spec


def build_definition(index, display_name, cargo_type, cache_class_path, resources, trucks, tiers, truck_speed_fraction=0.5):
    path = DATA + "DA_Convoy_Ore_%d" % index
    da = fresh_duplicate(CONVOY_TEMPLATE, path)
    da.set_editor_property("display_name", display_name)
    da.set_editor_property("trucks", trucks)
    da.set_editor_property("guard_tiers", tiers)
    cargo = da.get_editor_property("cargo")
    cargo.set_editor_property("cargo_type", cargo_type)
    cargo.set_editor_property("cache_class", unreal.load_class(None, cache_class_path))
    cargo.set_editor_property("resources_per_guard_point", resources)
    da.set_editor_property("cargo", cargo)
    speed = da.get_editor_property("speed")
    speed.set_editor_property("truck_speed_fraction", truck_speed_fraction)
    da.set_editor_property("speed", speed)
    save(path)
    log("definition %s: %s trucks=%d tiers=%d" % (path.split("/")[-1], display_name,
                                                  sum(t.get_editor_property("count") for t in trucks), len(tiers)))
    return da


def build_definitions(cargo_truck, fuel_truck, case_truck):
    zil_ammo, zil_fuel = tank(T.TANK_ZIL_AMMO), tank(T.TANK_ZIL_FUEL)
    cargo_speed, fuel_speed = 1372.0, 1006.0       # the calibrated Zil speeds of DA_Convoy_VehicleParts / _Fuel
    ba14, ba12, bt7, t26, t70 = tank(T.TANK_BA14), tank(T.TANK_BA12), tank(T.TANK_BT7), tank(T.TANK_T26), tank(T.TANK_T70)
    t34, t34_85, su76, su85, kv1 = tank(T.TANK_T34_76), tank(T.TANK_T34_85), tank(T.TANK_SU_76), tank(T.TANK_SU_85), tank(T.TANK_KV_1)
    mosin, okhotnik, maxim = squad(S.SQUAD_RUS_MOSIN), squad(S.SQUAD_RUS_OKHOTNIK), squad(S.SQUAD_RUS_MAXIM)
    L, R, LF, RF = ST.LEAD, ST.REAR, ST.LEFT_FLANK, ST.RIGHT_FLANK
    # 1: light fuel run (guided first ambush): 3-4 fuel trucks, a car or two
    build_definition(1, "Fuel convoy", CARGO.FUEL, CACHE_RADIXITE, {RES.RESOURCE_RADIXITE: 40.0},
                     [truck_spec(fuel_truck, zil_fuel, 3, fuel_speed)],
                     [tier("Light", 0, [escort(ba14, L), escort(mosin, R)]),
                      tier("Medium", 1, [escort(ba14, L), escort(ba14, R), escort(mosin, LF)]),
                      tier("Heavy", 1, [escort(ba14, L), escort(ba12, R), escort(bt7, RF), escort(mosin, LF)])])
    # 2: weighbridge convoy: 4-5 trucks parts + fuel, light armoured cars
    build_definition(2, "Supply convoy", CARGO.VEHICLE_PARTS, CACHE_PARTS, {RES.RESOURCE_VEHICLE_PARTS: 30.0},
                     [truck_spec(cargo_truck, zil_ammo, 2, cargo_speed),
                      truck_spec(fuel_truck, zil_fuel, 2, fuel_speed, CARGO.FUEL)],
                     [tier("Light", 0, [escort(ba14, L), escort(ba14, R), escort(mosin, LF)]),
                      tier("Medium", 1, [escort(ba14, L), escort(ba12, R), escort(ba14, RF), escort(mosin, LF), escort(okhotnik, R)]),
                      tier("Heavy", 1, [escort(ba12, L), escort(ba12, R), escort(ba14, RF, 2), escort(mosin, LF), escort(okhotnik, R)])])
    # 3: the ammo split: ammo trucks in front turn off to the battery, parts + fuel drive west; armoured cars
    build_definition(3, "Ammo convoy", CARGO.FUEL, CACHE_RADIXITE, {RES.RESOURCE_RADIXITE: 35.0},
                     [truck_spec(cargo_truck, zil_ammo, 2, cargo_speed, CARGO.AMMO),
                      truck_spec(cargo_truck, zil_ammo, 1, cargo_speed, CARGO.VEHICLE_PARTS),
                      truck_spec(fuel_truck, zil_fuel, 1, fuel_speed)],
                     [tier("Light", 0, [escort(ba14, L), escort(ba12, R), escort(mosin, LF)]),
                      tier("Medium", 1, [escort(ba14, L), escort(ba12, R), escort(ba14, RF), escort(mosin, LF), escort(mosin, RF)]),
                      tier("Heavy", 1, [escort(ba12, L), escort(ba12, R), escort(bt7, RF), escort(ba14, LF), escort(mosin, LF, 2),
                                        escort(okhotnik, R)])])
    # 4: hunters: trucks carry squads (set on the route: Hunters.SquadsPerTruck), armoured cars + a light tank
    build_definition(4, "Supply convoy", CARGO.VEHICLE_PARTS, CACHE_PARTS, {RES.RESOURCE_VEHICLE_PARTS: 30.0},
                     [truck_spec(cargo_truck, zil_ammo, 4, cargo_speed)],
                     [tier("Light", 0, [escort(ba14, L), escort(t70, R), escort(ba12, RF)]),
                      tier("Medium", 1, [escort(ba14, L), escort(t70, R), escort(ba12, RF), escort(ba14, LF), escort(mosin, LF)]),
                      tier("Heavy", 1, [escort(ba12, L), escort(t70, R), escort(t70, RF), escort(ba14, LF), escort(bt7, R),
                                        escort(mosin, LF)])])
    # 5: the case convoy: looks ordinary; the armoured case truck second in the column, concrete + parts, medium escort
    build_definition(5, "Supply convoy", CARGO.CONCRETE, CACHE_PARTS, {RES.RESOURCE_METAL: 30.0},
                     [truck_spec(cargo_truck, zil_ammo, 1, cargo_speed, CARGO.VEHICLE_PARTS),
                      truck_spec(case_truck, zil_ammo, 1, cargo_speed, CARGO.VEHICLE_PARTS),
                      truck_spec(cargo_truck, zil_ammo, 2, cargo_speed)],
                     [tier("Light", 0, [escort(ba12, L), escort(bt7, R), escort(t26, RF), escort(mosin, LF), escort(okhotnik, R)]),
                      tier("Medium", 1, [escort(ba12, L), escort(t34, R), escort(bt7, RF), escort(ba14, LF), escort(mosin, LF),
                                         escort(okhotnik, R)]),
                      tier("Heavy", 1, [escort(t34, L), escort(t34, R), escort(bt7, RF), escort(ba12, LF), escort(mosin, LF),
                                        escort(maxim, R), escort(okhotnik, RF)])])
    # 6: the heavy column to Base B: fuel + concrete, tanks
    build_definition(6, "Heavy column", CARGO.FUEL, CACHE_RADIXITE, {RES.RESOURCE_RADIXITE: 35.0},
                     [truck_spec(fuel_truck, zil_fuel, 2, fuel_speed),
                      truck_spec(cargo_truck, zil_ammo, 3, cargo_speed, CARGO.CONCRETE)],
                     [tier("Light", 0, [escort(t34, L), escort(t34, R), escort(ba12, RF), escort(mosin, LF), escort(okhotnik, R)]),
                      tier("Medium", 1, [escort(t34_85, L), escort(t34, R), escort(t34, RF), escort(su76, LF), escort(mosin, LF),
                                         escort(maxim, R)]),
                      tier("Heavy", 1, [escort(kv1, L), escort(t34_85, R), escort(t34_85, RF), escort(su85, LF), escort(t34, R),
                                        escort(mosin, LF), escort(okhotnik, RF), escort(maxim, R)])],
                     truck_speed_fraction=0.45)


def main():
    # definitions first (they reference the trucks), then trucks before their masters, then the rest
    for path in [DATA + "DA_Convoy_Ore_%d" % i for i in range(1, 7)]:
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
    for name in ["BP_Ore_CargoTruck", "BP_Ore_FuelTruck",
                                                                   "BP_Ore_CaseTruck", "BP_Ore_TruckMaster",
                                                                   "BP_Ore_CaseTruckMaster", "BP_Ore_SealedCases",
                                                                   "SM_Ore_WhiteFlag", "MI_Ore_WhiteFlag"]:
        if EAL.does_asset_exist(BASE + name):
            EAL.delete_asset(BASE + name)
    flag = build_white_flag()
    cases = build_sealed_cases()
    cases_class = cases.generated_class()
    master = build_master("BP_Ore_TruckMaster", unreal.SurrenderingCargoTruck, flag)
    case_master = build_master("BP_Ore_CaseTruckMaster", unreal.SurrenderingCaseTruck, flag, cases_class)
    cargo_truck = build_truck("BP_Ore_CargoTruck", ZIL_AMMO, master)
    fuel_truck = build_truck("BP_Ore_FuelTruck", ZIL_FUEL, master)
    case_truck = build_truck("BP_Ore_CaseTruck", ZIL_AMMO, case_master, cases_class)
    build_definitions(cargo_truck, fuel_truck, case_truck)
    log("DONE")


main()
