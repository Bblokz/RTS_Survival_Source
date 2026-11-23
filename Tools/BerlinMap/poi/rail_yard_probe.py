"""Read-only probe for the rail-yard D1 build (BerlinRailYard.md, owner's pick 2026-11-22).

Run in a DebugGame commandlet (no GPU):
    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -nullrhi -unattended
Writes Saved/BerlinRailYard/probe.json:
  - assets: every static mesh component of the wall, gate, tower, track, target and dressing Blueprints with its
    relative transform and mesh bounds (actor-local box), plus the RTS component's default owner;
  - map: actors in and round the yard, the BERW_Rails instances near the rail_east line and the yard spur, actor
    counts per prefix (FORGE_, RES_, POI_, BER_Yard) and ground heights on a 2 m grid (line traces).
Nothing is saved.
"""
import json
import math
import os

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/BerlinRailYard/probe.json"
MAP = "/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin"
W = "/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/"
B = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/"
ENV = "/Game/RTS_Survival/Blueprints/Environment/"
ASSETS = {
    "wall_long": W + "Rus_Wall_Concrete_Long",
    "wall_long_soviet": W + "Rus_Wall_Concrete_Long_Soviet",
    "wall_corner": W + "Rus_Wall_Concrete_Corner",
    "wall_corner_noflag": W + "Rus_Wall_Concrete_CornerNoFlag",
    "wall_sand": W + "Rus_Wall_Concrete_LongSandBags",
    "wall_sand_noflag": W + "Rus_Wall_Concrete_LongSandBagsNoFlag",
    "wall_holes": W + "Rus_Wall_Concrete_Long_Holes",
    "gate_a": W + "Gates/BP_RusWallGateA",
    "gate_c": W + "Gates/BP_RusWallGateC",
    "tower_mg": B + "Rus_BXP_GuardTowerMG_Soviet",
    "road_spline": ENV + "Splines/BP_RoadSpline",
    "scav_base": ENV + "ScavengeObjects/BP_ScavengeableObject",
    "scav_steeltower": ENV + "ScavengeObjects/SteelTowers/BP_ScavObj_SteelTower_001",
    "drainpipes": ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_DrainPipeStack",
    "fence_long": ENV + "DestructableEnvActor/Military/ConcreteWall/BP_Dest_PostApo_ConcreteFenceLong",
}
MESHES = {
    "rail": "/Game/RTS_Survival/Meshes/Buildings/Industrial/RailRoad/SM_RustedRefinery_RailRoad",
    "old_rail": "/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01",
    "ramp": ENV + "Berlin/RailYardTarget/SM_Berlin_RailRamp",
    "switch_tower": ENV + "Berlin/RailYardTarget/SM_Berlin_SwitchTower",
}
BOX = (-9000.0, 3500.0, 8000.0, 27000.0)          # X0, X1, Y0, Y1 (cm) round the yard
RAIL_EAST = [(-10833.0, 50000.0), (-11917.0, 46444.0), (-6500.0, 38889.0), (-2492.0, 31000.0), (-1842.0, 23333.0),
             (-1842.0, 18056.0), (-1842.0, 12700.0)]


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def probe_bp(path):
    bp = unreal.load_asset(path)
    if bp is None:
        return {"missing": path}
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    try:
        parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp) if hasattr(
            unreal.BlueprintEditorLibrary, "get_blueprint_parent_class") else None
    except Exception:
        parent = None
    out = {"parent": parent.get_name() if parent else str(bp.generated_class().get_super_class().get_name()
                                                          if hasattr(bp.generated_class(), "get_super_class") else ""),
           "components": []}
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object_for_blueprint(lib.get_data(handle), bp)
        if obj is None:
            continue
        row = {"name": obj.get_name(), "cls": obj.get_class().get_name()}
        if isinstance(obj, unreal.SceneComponent):
            row["loc"] = vec(obj.get_editor_property("relative_location"))
            r = obj.get_editor_property("relative_rotation")
            row["rot"] = [round(r.pitch, 1), round(r.yaw, 1), round(r.roll, 1)]
            row["scale"] = vec(obj.get_editor_property("relative_scale3d"))
            try:
                row["attach"] = str(obj.get_attach_parent().get_name()) if obj.get_attach_parent() else None
            except Exception:
                pass
        if isinstance(obj, unreal.StaticMeshComponent) and not isinstance(obj, unreal.InstancedStaticMeshComponent):
            mesh = obj.get_editor_property("static_mesh")
            if mesh:
                box = mesh.get_bounding_box()
                row.update(mesh=mesh.get_path_name(), min=vec(box.min), max=vec(box.max),
                           nav=obj.get_editor_property("can_ever_affect_navigation"))
        if isinstance(obj, unreal.BoxComponent):
            row["extent"] = vec(obj.get_editor_property("box_extent"))
        out["components"].append(row)
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        rts = None
        for c in cdo.get_components_by_class(unreal.RTSComponent):
            rts = c
        if rts is not None:
            out["owning_player"] = rts.get_editor_property("owning_player")
        for prop in ("scavenge_time", "give_player_resource_rewards", "consume_squad_when_finished", "health"):
            try:
                out[prop] = cdo.get_editor_property(prop)
            except Exception:
                pass
    except Exception as e:
        out["cdo_error"] = str(e)
    return out


def probe_assets():
    res = {k: probe_bp(p) for k, p in ASSETS.items()}
    for k, p in MESHES.items():
        m = unreal.load_asset(p)
        if m is None:
            res[k] = {"missing": p}
            continue
        box = m.get_bounding_box()
        res[k] = {"min": vec(box.min), "max": vec(box.max),
                  "materials": [str(s.get_editor_property("material_interface").get_name()) if s.get_editor_property("material_interface") else None
                                for s in m.get_editor_property("static_materials")]}
    return res


def ground(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -5000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    comp = t[8] if len(t) > 8 else None
    return round(t[4].z, 1), (comp.get_owner().get_actor_label() if comp and comp.get_owner() else None)


def probe_map():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = unreal.EditorActorSubsystem and unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    counts = {}
    rows = []
    rails = []
    landscape_only = []
    for a in actors:
        label = a.get_actor_label()
        for pre in ("FORGE_", "RES_", "POI_", "BER_Yard", "BERW_", "RY_", "BER_", "SKIRT_", "CRATER_"):
            if label.startswith(pre):
                counts[pre] = counts.get(pre, 0) + 1
        loc = a.get_actor_location()
        if label == "BERW_Rails":
            for comp in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
                for i in range(comp.get_instance_count()):
                    t = comp.get_instance_transform(i, True)
                    p = (t.translation.x, t.translation.y)
                    d = min(seg_dist(p, a0, b0) for a0, b0 in zip(RAIL_EAST, RAIL_EAST[1:]))
                    rails.append({"i": i, "comp": comp.get_name(), "x": round(p[0]), "y": round(p[1]),
                                  "z": round(t.translation.z), "yaw": round(t.rotation.rotator().yaw, 1),
                                  "d_rail_east": round(d)})
            continue
        if BOX[0] <= loc.x <= BOX[1] and BOX[2] <= loc.y <= BOX[3]:
            origin, extent = a.get_actor_bounds(False)
            rows.append({"label": label, "cls": a.get_class().get_name(), "x": round(loc.x), "y": round(loc.y),
                         "z": round(loc.z), "yaw": round(a.get_actor_rotation().yaw, 1),
                         "bounds": [round(origin.x), round(origin.y), round(extent.x), round(extent.y), round(extent.z)],
                         "folder": str(a.get_folder_path())})
    heights = []
    for x in range(int(BOX[0]), int(BOX[1]) + 1, 200):
        for y in range(int(BOX[2]), int(BOX[3]) + 1, 200):
            g = ground(world, float(x), float(y))
            heights.append([x, y, g[0] if g else None, g[1] if g else None])
    mm = [a for a in actors if "MissionManager" in a.get_class().get_name()]
    return {"counts": counts, "actors": rows, "rails": rails, "heights": heights,
            "mission_manager": [m.get_actor_label() for m in mm]}


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    res = {"assets": probe_assets()}
    try:
        res["map"] = probe_map()
    except Exception as e:
        res["map_error"] = repr(e)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    unreal.log_warning("RAILYARD_PROBE done -> " + OUT)


main()
