"""Builds /Game/RTS_Survival/Blueprints/Environment/SupplyPoint/BP_SupplyPoint_TowerCrane (parent ASupplyPoint).

No graph logic: ASupplyPoint raises the flag and swaps its banner, USupplyPointCraneComponent animates the crane and the
stockpiles. Both find their parts by component NAME, so the names below are the contract.
Run headless on the DebugGame build (the only DLL with USupplyPointCraneComponent right now), under the DebugGame lock:
    bash Source/Tools/SupplyPoint/TruckBP/run_debuggame_py.sh <this file> supply_point_bp
Re-runnable: deletes and rebuilds the Blueprint. Prints SUPPLYBP lines; the last one is SUPPLYBP RESULT OK|FAIL.
Positions are the imported mesh sockets (Source/Tools/SupplyPoint/Model/ue_import_supply_point.py), UE cm and yaw.
"""
import math

import unreal

FOLDER = "/Game/RTS_Survival/Blueprints/Environment/SupplyPoint"
BP_NAME = "BP_SupplyPoint_TowerCrane"
M = FOLDER + "/Meshes/"
EMBLEMS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/Mat/Emblems/"
POLES = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/FactionFlags/"
SLEW = (0.0, -220.0, 675.0)
REST_SLEW_YAW = -200.0
REST_TROLLEY = 385.0
REST_HOOK_DROP = 55.0
FLAG_LOWERED = (-694.4, -790.2, 220.0)
FLAG_FLY_YAW = -215.8
STOCKS = (("Radixite", "Resource_Radixite", (-554.4, -421.8, 12.0), -70.0),
          ("Metal", "Resource_Metal", (0.0, -810.0, 12.0), 0.0),
          ("VehicleParts", "Resource_VehicleParts", (554.4, -421.8, 12.0), 70.0))
# Truck points: the park point is the Opel actor origin (its mesh origin, 5 cm behind the bounding-box centre).
TRUCK_POINTS = {"TruckApproachPoint": ((-1100.0, -150.0, 0.0), 0.0),
                "TruckParkPoint": ((120.0, 330.0, 0.0), 0.0),
                "TruckExitPoint": ((1350.0, -150.0, 0.0), 0.0)}

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
OK = [True]


def log(*parts):
    unreal.log("SUPPLYBP " + " ".join(str(p) for p in parts))


def fail(msg):
    OK[0] = False
    log("FAIL", msg)


def enum_value(enum_cls, wanted):
    key = wanted.replace("_", "").lower()
    for name in dir(enum_cls):
        if name.replace("_", "").lower() == key:
            return getattr(enum_cls, name)
    raise RuntimeError(f"{wanted} not in {enum_cls}")


def templates(blueprint):
    found = {}
    for handle in sds.k2_gather_subobject_data_for_blueprint(blueprint):
        obj = lib.get_object_for_blueprint(sds.k2_find_subobject_data_from_handle(handle), blueprint)
        if obj:
            found.setdefault(obj.get_name().replace("_GEN_VARIABLE", ""), (handle, obj))
    return found


def add(blueprint, name, cls, parent_handle):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=blueprint)
    handle, reason = sds.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"add {name} failed: {reason}")
    sds.rename_subobject(handle, unreal.Text(name))
    return handle, lib.get_object_for_blueprint(sds.k2_find_subobject_data_from_handle(handle), blueprint)


def place(comp, loc=(0.0, 0.0, 0.0), rot=None, scale=(1.0, 1.0, 1.0)):
    comp.set_editor_property("relative_location", unreal.Vector(*loc))
    comp.set_editor_property("relative_rotation", rot or unreal.Rotator(0.0, 0.0, 0.0))
    comp.set_editor_property("relative_scale3d", unreal.Vector(*scale))


def mesh_component(blueprint, name, parent_handle, mesh, loc=(0, 0, 0), rot=None, scale=(1, 1, 1), collide=False):
    handle, comp = add(blueprint, name, unreal.StaticMeshComponent, parent_handle)
    comp.set_editor_property("static_mesh", unreal.load_asset(mesh))
    comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    place(comp, loc, rot, scale)
    if not collide:
        comp.set_collision_profile_name("NoCollision")
        comp.set_editor_property("can_ever_affect_navigation", False)
    return handle, comp


def flag_rotation():
    """Banner local +X (hoist -> fly) along the fly yaw, local +Y (bottom -> top edge) up; checked upright in Unreal."""
    a = math.radians(FLAG_FLY_YAW)
    return unreal.MathLibrary.make_rot_from_xy(unreal.Vector(math.cos(a), math.sin(a), 0.0), unreal.Vector(0, 0, 1))


def resource_setup(kind, stock_name):
    """EditDefaultsOnly struct fields refuse set_editor_property on a struct instance: build it with import_text."""
    def obj(path):
        name = path.rsplit("/", 1)[1]
        return f'"{path}.{name}"'
    levels = ",".join(f"(Level={lv},Mesh={obj(M + f'Fills/SM_SupplyPoint_{kind}At{lv}')})" for lv in (100, 75, 50, 25, 10))
    text = (f'(StockComponentName="{stock_name}",StockLevels=({levels}),PickSocketName="Pick_{kind}",'
            f'LoadMesh={obj(M + f"Loads/SM_SupplyLoad_{kind}")},TruckCargoSlotName="CargoSlot_{kind}",StockLevelAfterLift=50)')
    setup = unreal.SupplyCraneResourceSetup()
    setup.import_text(text)
    return setup


def build():
    path = f"{FOLDER}/{BP_NAME}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.SupplyPoint)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, FOLDER, unreal.Blueprint, factory)
    comps = templates(blueprint)
    log("inherited", sorted(comps))
    actor_handle = sds.k2_gather_subobject_data_for_blueprint(blueprint)[0]
    root_handle = comps["SupplyPointRoot"][0]
    for name, (loc, yaw) in TRUCK_POINTS.items():
        place(comps[name][1], loc, unreal.Rotator(0.0, 0.0, yaw))

    mesh_component(blueprint, "Base", root_handle, M + "SM_SupplyPoint_Base", collide=True)
    up_h, _ = mesh_component(blueprint, "UpperWorks", root_handle, M + "SM_SupplyPoint_UpperWorks", SLEW,
                             unreal.Rotator(0.0, 0.0, REST_SLEW_YAW))
    tr_h, _ = mesh_component(blueprint, "Trolley", up_h, M + "SM_SupplyPoint_Trolley", (REST_TROLLEY, 0.0, 0.0))
    mesh_component(blueprint, "Rope", tr_h, M + "SM_SupplyPoint_Rope", scale=(1.0, 1.0, REST_HOOK_DROP / 100.0))
    mesh_component(blueprint, "HookBlock", tr_h, M + "SM_SupplyPoint_HookBlock", (0.0, 0.0, -REST_HOOK_DROP))
    pole_h, pole = add(blueprint, "FlagPole", unreal.SceneComponent, root_handle)
    pole.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    place(pole, FLAG_LOWERED)
    fr = flag_rotation()
    log("flag relative rotation", fr)
    mesh_component(blueprint, "Flag", pole_h, M + "SM_SupplyPoint_Flag", rot=fr)
    for kind, _, loc, yaw in STOCKS:
        mesh_component(blueprint, "Stock_" + kind, root_handle, M + f"Fills/SM_SupplyPoint_{kind}At100", loc,
                       unreal.Rotator(0.0, 0.0, yaw))
    _, crane = add(blueprint, "SupplyCrane", unreal.SupplyPointCraneComponent, actor_handle)
    setups = {}
    for kind, resource, _, _ in STOCKS:
        setups[enum_value(unreal.RTSResourceType, resource)] = resource_setup(kind, "Stock_" + kind)
    crane.set_editor_property("resource_setups", setups)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    settings = cdo.get_editor_property("supply_point_settings")
    settings.set_editor_property("flag_raise_height", 600.0)
    settings.set_editor_property("progress_bar_offset", unreal.Vector(0.0, 0.0, 1050.0))
    cdo.set_editor_property("supply_point_settings", settings)
    cdo.set_editor_property("enemy_flag_materials", {
        enum_value(unreal.EnemyDirector, "DirectorOfShield"): unreal.load_asset(EMBLEMS + "MI_Emblem_DirectorOfShield"),
        enum_value(unreal.EnemyDirector, "DirectorOfForge"): unreal.load_asset(EMBLEMS + "MI_Emblem_DirectorOfForge"),
        enum_value(unreal.EnemyDirector, "DirectorOfHarvest"): unreal.load_asset(EMBLEMS + "MI_Emblem_DirectorOfHarvest"),
    })
    cdo.set_editor_property("player_flag_materials", {
        enum_value(unreal.RTSFaction, "GerBreakthroughDoctrine"): unreal.load_asset(POLES + "MI_BreakthroughPole"),
        enum_value(unreal.RTSFaction, "GerStrikeDivision"): unreal.load_asset(POLES + "MI_StrikeDivisionPole"),
    })
    cdo.set_editor_property("flag_component_name", "Flag")
    cdo.set_editor_property("flag_material_slot", 0)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    saved = eal.save_loaded_asset(blueprint, False)
    log("saved", saved, blueprint.get_path_name())
    if not saved:
        fail("save")
    return blueprint


def verify(blueprint):
    cdo = unreal.get_default_object(blueprint.generated_class())
    s = cdo.get_editor_property("supply_point_settings")
    log("settings flag_raise_height", s.get_editor_property("flag_raise_height"), "progress_bar_offset",
        s.get_editor_property("progress_bar_offset"))
    log("enemy flags", {str(k): v.get_name() for k, v in cdo.get_editor_property("enemy_flag_materials").items()})
    log("player flags", {str(k): v.get_name() for k, v in cdo.get_editor_property("player_flag_materials").items()})
    log("flag name", cdo.get_editor_property("flag_component_name"), "slot", cdo.get_editor_property("flag_material_slot"))
    want = ["Base", "UpperWorks", "Trolley", "Rope", "HookBlock", "FlagPole", "Flag", "Stock_Radixite", "Stock_Metal",
            "Stock_VehicleParts", "SupplyCrane", "TruckApproachPoint", "TruckParkPoint", "TruckExitPoint"]
    comps = templates(blueprint)
    for name in want:
        if name not in comps:
            fail("missing component " + name)
            continue
        c = comps[name][1]
        if isinstance(c, unreal.SceneComponent):
            parent = c.get_attach_parent()
            log("comp", name, c.get_class().get_name(), "loc", c.get_editor_property("relative_location"),
                "rot", c.get_editor_property("relative_rotation"), "scale", c.get_editor_property("relative_scale3d"),
                "mobility", c.get_editor_property("mobility"))
        else:
            setups = c.get_editor_property("resource_setups")
            for k, v in setups.items():
                log("crane setup", k, v.get_editor_property("stock_component_name"), v.get_editor_property("pick_socket_name"),
                    v.get_editor_property("truck_cargo_slot_name"), v.get_editor_property("load_mesh").get_name(),
                    [e.get_editor_property("level") for e in v.get_editor_property("stock_levels")])
            log("crane settings", c.get_editor_property("crane_settings"))


bp = build()
verify(bp)
log("RESULT " + ("OK" if OK[0] else "FAIL"))
