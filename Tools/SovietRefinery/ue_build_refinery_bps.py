"""Builds the Soviet refinery Blueprints (A1, G2) and their crane-station Blueprints from the layout and rig JSON.

Contract: Source/Docs/EnemyAI/SovietRefinery.md. Run inside an editor (UECP execute_python or -ExecCmds "py <this>")
or a -run=pythonscript commandlet with the DebugGame/Development DLL that has USovietRefineryComponent and
ASkeletalCraneUnloadStation. Idempotent: existing Blueprints are updated, components are re-created by name.

    py "D:/UE5Projects/RTS_Survival/Source/Tools/SovietRefinery/ue_build_refinery_bps.py"
"""
import json
import os

import unreal

PROJECT = "D:/UE5Projects/RTS_Survival"
BUILD = PROJECT + "/_AssetSource/SovietRefinery/Build"
ROOT = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings"
REF_DIR = ROOT + "/SovietRefinery"
CRANE_MESH = REF_DIR + "/Crane/SK_SovRef_Crane"
HQ_STATION_BP = None  # found by name below (payload meshes are copied from the HQ station)

DESIGNS = {
    "A1": {"parent": ROOT + "/Rus_Zone_Refinery1", "folder": REF_DIR + "/A1",
           "static": ["SM_SovRef_A1_StorageHall", "SM_SovRef_A1_CranePad", "SM_SovRef_A1_Tripper"]},
    "G2": {"parent": ROOT + "/Rus_Zone_Refinery3", "folder": REF_DIR + "/G2",
           "static": ["SM_SovRef_G2_AnnexStorage", "SM_SovRef_G2_CranePlatform"]},
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
CRANE_STATION_CLASS = unreal.load_class(None, "/Script/RTS_Survival.SkeletalCraneUnloadStation")
REFINERY_COMPONENT_CLASS = unreal.load_class(None, "/Script/RTS_Survival.SovietRefineryComponent")


def log(msg):
    unreal.log("SOVREF_BP " + msg)


def vec(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]) if len(v) > 2 else 0.0)


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def ensure_child_bp(path, parent_class):
    if eal.does_asset_exist(path):
        bp = eal.load_asset(path)
        log("exists " + path)
        return bp
    folder, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = asset_tools.create_asset(name, folder, unreal.Blueprint, factory)
    log("created " + path)
    return bp


def gen_class(bp):
    return unreal.load_object(None, bp.get_path_name() + "_C") if bp else None


def handles_by_name(bp):
    out = {}
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = SDL.get_data(h)
        out[str(SDL.get_variable_name(data))] = h
    return out


def remove_named(bp, names):
    handles = handles_by_name(bp)
    root = list(handles.values())[0]
    doomed = [handles[n] for n in names if n in handles]
    if doomed:
        sds.delete_subobjects(root, doomed, bp)


def add_component(bp, parent_name, comp_class, name):
    handles = handles_by_name(bp)
    parent = handles.get(parent_name) or list(handles.values())[0]
    params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=comp_class, blueprint_context=bp)
    handle, fail = sds.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError("add %s failed: %s" % (name, fail))
    sds.rename_subobject(handle, unreal.Text(name))
    return SDL.get_object(SDL.get_data(handle))


def build_station_bp(design, layout, rig):
    path = REF_DIR + "/BP_SovRefCraneStation_" + design
    bp = ensure_child_bp(path, CRANE_STATION_CLASS)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(gen_class(bp))
    crane = layout["crane"]
    dock = layout["dock"]
    drops = layout["drop_points"]
    props = {
        "dock_pick_point": vec(dock["pick_point"]),
        "dock_yaw": float(dock.get("harvester_yaw_deg", 0.0)),
        "dock_approach_distance": float(dock.get("approach_distance", 900.0)),
        "radixite_drop_point": vec(drops["Radixite"]),
        "metal_drop_point": vec(drops["Metal"]),
        "crane_location": vec(crane["location"]),
        "crane_yaw": float(crane.get("yaw_deg", 0.0)),
        "crane_skeletal_mesh": eal.load_asset(CRANE_MESH),
        "use_overflow_drop_off_point": True,
        "overflow_drop_off_point": vec(layout["overflow_point"]),
    }
    # Rig facts (crane_rig.json): the C++ defaults already match SK_SovRef_Crane; re-apply the measured ones.
    if rig:
        props["luff_min"] = float(rig["luff"]["limits_deg"]["min"])
        props["luff_max"] = float(rig["luff"]["limits_deg"]["max"])
        props["luff_axis"] = vec(rig["luff"]["local_axis"])
        props["yaw_axis"] = vec(rig["slew"]["local_axis"])
        props["jaw_axis"] = vec(rig["jaws"]["jaw_l"]["local_axis"])
        props["jaw_left_sign"] = float(rig["jaws"]["jaw_l"]["open_sign"])
        props["jaw_right_sign"] = float(rig["jaws"]["jaw_r"]["open_sign"])
        props["jaw_bottom_below_head"] = -float(rig["grab_head"]["offset_to_closed_jaw_bottom_cm"][2])
    for prop, value in props.items():
        cdo.set_editor_property(prop, value)
    # Payload: the HQ station's crystal and ingot meshes.
    hq = find_hq_station_cdo()
    if hq:
        cdo.set_editor_property("crystal_mesh", hq.get_editor_property("crystal_mesh"))
        cdo.set_editor_property("bar_mesh", hq.get_editor_property("bar_mesh"))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp, False)
    log("station %s set (%d props)" % (path, len(props)))
    return bp


def find_hq_station_cdo():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in reg.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "Blueprint"), False):
        if str(data.asset_name) == "BP_HQUnloadStation":
            bp = data.get_asset()
            return unreal.get_default_object(gen_class(bp))
    return None


def build_refinery_bp(design, layout, station_bp):
    cfg = DESIGNS[design]
    parent_bp = eal.load_asset(cfg["parent"])
    path = REF_DIR + "/Rus_Building_SovietRefinery_" + design
    bp = ensure_child_bp(path, gen_class(parent_bp))

    names = ["SovietRefinery", "HarvesterExit"] + ["SovRef_" + n for n in cfg["static"]]
    names += ["SovRef_" + f["mesh"] for f in layout.get("fills", [])]
    names += ["SovRef_" + b["mesh"] for b in layout.get("bars", [])]
    remove_named(bp, names)

    # Static additions placed in the building frame (exported in place): identity under the building mesh.
    for mesh_name in cfg["static"]:
        mesh = eal.load_asset(cfg["folder"] + "/" + mesh_name)
        if not mesh:
            log("MISSING mesh " + mesh_name)
            continue
        comp = add_component(bp, "BuildingMeshComponent", unreal.StaticMeshComponent, "SovRef_" + mesh_name)
        comp.set_editor_property("static_mesh", mesh)
        if "Tripper" in mesh_name and "tripper" in layout:
            comp.set_editor_property("relative_location", vec(layout["tripper"]["rail_start"]))
            comp.set_collision_profile_name("NoCollision")
            comp.set_editor_property("component_tags", ["SovRefTripper"])
            comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)

    # Stock fills and level bars: pivot at the cell floor / bar bottom, scaled in Z by the refinery.
    for kind, entries in (("SovRefFill", layout.get("fills", [])), ("SovRefBar", layout.get("bars", []))):
        for entry in entries:
            mesh = eal.load_asset(cfg["folder"] + "/" + entry["mesh"])
            if not mesh:
                log("MISSING mesh " + entry["mesh"])
                continue
            comp = add_component(bp, "BuildingMeshComponent", unreal.StaticMeshComponent, "SovRef_" + entry["mesh"])
            comp.set_editor_property("static_mesh", mesh)
            comp.set_editor_property("relative_location", vec(entry["pivot"]))
            comp.set_collision_profile_name("NoCollision")
            comp.set_editor_property("can_ever_affect_navigation", False)
            comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            comp.set_editor_property("component_tags",
                                     ["%s_%s_%d" % (kind, entry["resource"], int(entry.get("order", 0)))])

    exit_cfg = layout["harvester_exit"]
    exit_comp = add_component(bp, "BuildingMeshComponent", unreal.SceneComponent, "HarvesterExit")
    exit_comp.set_editor_property("relative_location", vec(exit_cfg["location"]))
    exit_comp.set_editor_property("relative_rotation", unreal.Rotator(0.0, float(exit_cfg.get("yaw_deg", 0.0)), 0.0))
    exit_comp.set_editor_property("component_tags", ["SovRefHarvesterExit"])

    refinery = add_component(bp, None, REFINERY_COMPONENT_CLASS, "SovietRefinery")
    refinery.set_editor_property("unload_station_class", gen_class(station_bp))
    if "tripper" in layout:
        refinery.set_editor_property("tripper_rail_start", vec(layout["tripper"]["rail_start"]))
        refinery.set_editor_property("tripper_rail_end", vec(layout["tripper"]["rail_end"]))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp, False)
    log("refinery %s built" % path)
    return bp


def main():
    rig_path = BUILD + "/Crane/crane_rig.json"
    rig = load_json(rig_path) if os.path.exists(rig_path) else {}
    for design in ("A1", "G2"):
        layout_path = "%s/%s/layout.json" % (BUILD, design)
        if not os.path.exists(layout_path):
            log("MISSING " + layout_path)
            continue
        layout = load_json(layout_path)
        station = build_station_bp(design, layout, rig)
        build_refinery_bp(design, layout, station)
    log("done")


main()
