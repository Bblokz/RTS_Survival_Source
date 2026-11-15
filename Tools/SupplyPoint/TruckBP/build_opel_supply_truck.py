"""Builds /Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/BP_Ger_OpelSupplyTruck from the Zil fuel truck.

Run headless on the DebugGame build (the Development editor DLL does not know Tank_OpelSupplyTruck):
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nullrhi
Hold Saved/AgentLocks/debuggame_build.lockdir while it runs. Re-runnable: it deletes and rebuilds the folder's assets.

Layout: TrackMesh (inherited skeletal root) = SK_Ger_Opel + AnimBp_Opel, which is the whole visible truck.
Hull (static) = an invisible copy of the Opel training-preview mesh: it takes weapon traces for ArmorCalculation
and carries the Exhaust / EngineFire sockets, like SM_Hull_Zil does on the template.
"""
import unreal

ZIL = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilCargo/"
OPEL = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/"
DEST = "/Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/"
TRUCK = DEST + "BP_Ger_OpelSupplyTruck"
WRECK = DEST + "Env_Dest_OpelSupplyTruck"
HIT_HULL = DEST + "SM_OpelSupplyTruck_HitHull"
WRECK_MESH = OPEL + "Skeleton/SM_OpelDestroyed"
WRECK_MATERIAL = "/Game/RTS_Survival/Materials/Destroyed/MI_RTS_Destroyed_Metal"
DRIVER_MESH = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Mesh/SK_PostApoSoldier"

# Truck-local cm, in the Opel mesh's own space (TrackMesh is the root at scale 1).
BED_FLOOR_Z = 118.0
CARGO_SLOTS = (("CargoSlot_Radixite", -23.0), ("CargoSlot_Metal", -123.0), ("CargoSlot_VehicleParts", -224.0))
# SK_Ger_Opel's own Exhaust socket (tail pipe under the rear of the bed).
EXHAUST_LOCATION = unreal.Vector(-304.56, 65.63, 78.31)
EXHAUST_ROTATION = unreal.Rotator(0.0, 30.0, 0.0)  # (roll, pitch, yaw)
ENGINE_FIRE_LOCATION = unreal.Vector(250.0, 0.0, 150.0)
EXPLOSION_LOCATION = unreal.Vector(120.0, 0.0, 180.0)

# The copied graph still passes the Zil's NewAnimationSpeedDivider (150) to InitWheeledAnimationInstance; the Opel
# animations roll true at 200 (what the Opel nomadic trucks pass). Python cannot edit graph pins, so the wheel
# animation is slowed on the component instead: 150 / 200. Set this back to 1 when the pin is changed to 200.
WHEEL_ANIM_RATE_SCALE = 0.75

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary


def log(*parts):
    unreal.log("OPELTRUCK " + " ".join(str(p) for p in parts))


def fresh_duplicate(source, destination):
    if eal.does_asset_exist(destination):
        eal.delete_asset(destination)
    asset = eal.duplicate_asset(source, destination)
    if not asset:
        raise RuntimeError(f"duplicate failed: {source} -> {destination}")
    return asset


def templates(blueprint):
    """name (without _GEN_VARIABLE) -> (handle, template object as seen by this Blueprint)."""
    found = {}
    for handle in sds.k2_gather_subobject_data_for_blueprint(blueprint):
        data = sds.k2_find_subobject_data_from_handle(handle)
        obj = lib.get_object_for_blueprint(data, blueprint)
        if obj:
            found.setdefault(obj.get_name().replace("_GEN_VARIABLE", ""), (handle, obj))
    return found


def root_handle(blueprint):
    return sds.k2_gather_subobject_data_for_blueprint(blueprint)[0]


def delete_components(blueprint, names):
    comps = templates(blueprint)
    handles = [comps[n][0] for n in names if n in comps]
    removed = sds.delete_subobjects(root_handle(blueprint), handles, blueprint)
    log("deleted", removed, "of", names)


def add_socket(mesh, name, location, rotation=unreal.Rotator(0.0, 0.0, 0.0)):
    socket = unreal.StaticMeshSocket(outer=mesh)
    socket.set_editor_property("socket_name", name)
    socket.set_editor_property("relative_location", location)
    socket.set_editor_property("relative_rotation", rotation)
    mesh.add_socket(socket)


def build_hit_hull():
    mesh = fresh_duplicate(OPEL + "Skeleton/OpelTrainingPreview", HIT_HULL)
    # Same trace behaviour as SM_Hull_Zil: weapons trace the real triangles.
    mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    add_socket(mesh, "Exhaust", EXHAUST_LOCATION, EXHAUST_ROTATION)
    add_socket(mesh, "EngineFire", ENGINE_FIRE_LOCATION)
    add_socket(mesh, "Exp1", EXPLOSION_LOCATION)
    eal.save_loaded_asset(mesh, False)
    log("hit hull sockets", [str(s.socket_name) for s in mesh.get_sockets_by_tag("")])
    return mesh


def build_wreck():
    blueprint = fresh_duplicate(ZIL + "Env_Dest_Zil_Fuel", WRECK)
    delete_components(blueprint, [f"Cargo_{i}" for i in range(6)])
    hull = templates(blueprint)["Hull"][1]
    hull.set_editor_property("static_mesh", unreal.load_asset(WRECK_MESH))
    hull.set_editor_property("override_materials", [unreal.load_asset(WRECK_MATERIAL)])
    hull.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    eal.save_loaded_asset(blueprint, False)
    return blueprint


def opel_subtype():
    for name in dir(unreal.TankSubtype):
        if "OPEL" in name.upper() and "SUPPLY" in name.upper():
            return getattr(unreal.TankSubtype, name)
    raise RuntimeError("Tank_OpelSupplyTruck is not in this build's ETankSubtype: run on the DebugGame editor")


def build_truck(hit_hull):
    blueprint = fresh_duplicate(ZIL + "Rus_ArmoredCar_ZilFuel", TRUCK)
    delete_components(blueprint, [f"Cargo_{i}" for i in range(6)])
    comps = templates(blueprint)

    track_mesh = comps["TrackMesh"][1]
    track_mesh.set_editor_property("skeletal_mesh_asset", unreal.load_asset(OPEL + "Skeleton/SK_Ger_Opel"))
    track_mesh.set_editor_property("anim_class", unreal.load_asset(OPEL + "AnimBp_Opel").generated_class())
    track_mesh.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    track_mesh.set_editor_property("global_anim_rate_scale", WHEEL_ANIM_RATE_SCALE)

    hull = comps["Hull"][1]
    hull.set_editor_property("static_mesh", hit_hull)
    for prop, value in (("visible", False), ("hidden_in_game", True), ("cast_shadow", False),
                        ("render_in_main_pass", False), ("render_in_depth_pass", False),
                        ("visible_in_ray_tracing", False), ("affect_distance_field_lighting", False)):
        hull.set_editor_property(prop, value)

    driver = comps["Driver"][1]
    driver.set_editor_property("skeletal_mesh_asset", unreal.load_asset(DRIVER_MESH))
    driver.set_editor_property("relative_location", unreal.Vector(101.0, -42.0, 58.0))

    comps["NS_Exhaust"][1].set_editor_property("relative_scale3d", unreal.Vector(0.25, 0.25, 0.25))
    comps["EngineSound"][1].set_editor_property("relative_location", unreal.Vector(230.0, 0.0, 110.0))
    comps["NS_TrackSmokes"][1].set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))

    comps["RTS"][1].set_editor_property("owning_player", 1)
    area = comps["Selectionarea"][1]
    area.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 120.0))
    area.set_editor_property("relative_scale3d", unreal.Vector(10.5, 4.75, 4.0))
    decal = comps["SelectedDecal"][1]
    decal.set_editor_property("relative_location", unreal.Vector(-12.0, 0.0, -10.5))
    decal.set_editor_property("relative_scale3d", unreal.Vector(2.0, 8.0, 13.0))
    comps["RTSNavCollisionInst"][1].set_editor_property("failsafe_extent", unreal.Vector(321.0, 122.0, 127.0))

    for name, x in CARGO_SLOTS:
        params = unreal.AddNewSubobjectParams(parent_handle=comps["TrackMesh"][0], new_class=unreal.SceneComponent,
                                              blueprint_context=blueprint)
        handle, fail_reason = sds.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"add {name} failed: {fail_reason}")
        sds.rename_subobject(handle, unreal.Text(name))
        slot = lib.get_object_for_blueprint(sds.k2_find_subobject_data_from_handle(handle), blueprint)
        slot.set_editor_property("relative_location", unreal.Vector(x, 0.0, BED_FLOOR_Z))
        log("added", name, slot.get_path_name())

    # Death graph: the DestroySpawnActor pin still names the Zil wreck class.
    old_wreck, new_wreck = ZIL + "Env_Dest_Zil_Fuel", WRECK
    redirects = {
        unreal.SoftObjectPath(f"{old_wreck}.Env_Dest_Zil_Fuel_C"): unreal.SoftObjectPath(f"{new_wreck}.Env_Dest_OpelSupplyTruck_C"),
        unreal.SoftObjectPath(f"{old_wreck}.Env_Dest_Zil_Fuel"): unreal.SoftObjectPath(f"{new_wreck}.Env_Dest_OpelSupplyTruck"),
    }
    unreal.AssetToolsHelpers.get_asset_tools().rename_referencing_soft_object_paths(
        [blueprint.get_outermost()], redirects)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("ArmoredCarType", opel_subtype())
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    log("subtype after compile", cdo.get_editor_property("ArmoredCarType"))
    eal.save_loaded_asset(blueprint, False)
    return blueprint


def main():
    hit_hull = build_hit_hull()
    build_wreck()
    build_truck(hit_hull)
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    log("dirty packages left", dirty)
    log("BUILD_DONE")


main()
