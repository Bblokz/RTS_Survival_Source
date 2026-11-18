"""Read-only probe of Ger_IntroMission for the Soviet cargo train (ASovietCargoTrain).

Run in a THROWAWAY Development editor (never saves the map, quits when done):
    UnrealEditor.exe D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
        -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/train_path_probe.py"
        -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>

Or send it to a running train_editor_server.py with `python train_cmd.py train_path_probe.py` (runs at once, no quit).

Writes next to this script:
    train_path.json   {"points": [[x,y,z],...], "junction_index": n, ...}  main line north end -> junction -> siding end
    train_probe.json  rail spline raw data, parked train cars, edge blockers/skirts, candidate mesh bounds/materials
"""
import json
import math
import os
import traceback

import unreal

HERE = "D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack"
MAP_NAME = "Ger_IntroMission"
SAMPLE_STEP = 300.0
# Front of the locomotive stops here (~10 m north of REAR_Station_Buffer_Siding); the mission setup reads this key.
STOP_FRONT_LOCATION = [21000.0, 8500.0, 945.0]
STATE = {"ticks": 0, "done": False, "handle": None}

TRAIN = "/Game/RTS_Survival/Blueprints/GroundVehicles/Trains/Soviet/Meshes/"
VEH = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
TUR = "/Game/RTS_Survival/Blueprints/Turrets/Russian/"
MESHES = [
    TRAIN + "SM_SovietTrain_Locomotive", TRAIN + "SM_SovietTrain_CargoCar", TRAIN + "SM_SovietTrain_FuelCar",
    TRAIN + "SM_Cart_HowitzerTrain", "/Game/Hangar/Meshes/SM_TrainBoxCarGreen",
    VEH + "Rus_Medium/TrainingPreviews/SM_TrainingPreview_T34", VEH + "Rus_Medium/TrainingPreviews/SM_TrainingPreview_T34E",
    VEH + "Rus_Medium/T34/Meshes/SM_T34_Hull", VEH + "Rus_Medium/T34/Meshes/SM_T-34E_Hull",
    VEH + "Rus_Heavy/KV1/Meshes/SM_KV1_Hull", VEH + "Rus_Heavy/IS-1And-2/Meshes/SM_IS_1_Hull",
    TUR + "Medium/T34Turrets/Meshes/SM_T34_76_TurretBaseMesh", TUR + "Medium/T34Turrets/Meshes/SM_T3485_TurretBase_T34_85_Tower",
    TUR + "Heavy/KV1/SM_KV-1_Turret", TUR + "Heavy/IS-2/Meshes/SM_IS-2_BaseMesh",
    "/Game/RTS_Survival/Blueprints/Environment/FrozenLake/BurntConvoy/Meshes/SM_FL_ConvoyC_T34_1",
]


def v3(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground_z(w, x, y):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 5000.0), unreal.Vector(x, y, -5000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    if not t[0]:
        return None
    actor = t[9]
    return round(t[4].z, 1), (actor.get_actor_label() if actor else "")


def sample_spline(sp):
    length = sp.get_spline_length()
    n = max(2, int(math.ceil(length / SAMPLE_STEP)) + 1)
    pts = []
    for i in range(n):
        d = min(length, i * length / (n - 1))
        pts.append(sp.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD))
    return length, pts


def mesh_info(path):
    m = unreal.load_asset(path)
    if m is None:
        return {"path": path, "missing": True}
    b = m.get_bounds()
    info = {"path": m.get_path_name(), "origin": v3(b.origin), "extent": v3(b.box_extent)}
    try:
        info["materials"] = [str(s.material_interface.get_path_name()) if s.material_interface else None
                             for s in m.get_editor_property("static_materials")]
        sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        info["lods"] = sms.get_lod_count(m)
        info["verts_lod0"] = sms.get_number_verts(m, 0)
        info["nanite"] = bool(m.get_editor_property("nanite_settings").enabled)
    except Exception as exc:  # noqa
        info["err"] = str(exc)
    return info


def run():
    w = world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    by_label = {a.get_actor_label(): a for a in actors}
    probe = {"map": w.get_name(), "rails": {}, "parked": [], "edges": [], "meshes": []}
    rails = {}
    for label in ("REAR_Rail_Main", "REAR_Rail_Siding"):
        a = by_label.get(label)
        sp = a.get_component_by_class(unreal.SplineComponent)
        length, pts = sample_spline(sp)
        ctrl = [v3(sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD))
                for i in range(sp.get_number_of_spline_points())]
        dz = []
        for p in pts[::5]:
            g = ground_z(w, p.x, p.y)
            if g:
                dz.append(round(p.z - g[0], 1))
        rails[label] = pts
        probe["rails"][label] = {"length": round(length, 1), "control_points": ctrl, "samples": len(pts),
                                 "spline_minus_ground_every_15m": dz,
                                 "closed": sp.is_closed_loop()}
    main, siding = rails["REAR_Rail_Main"], rails["REAR_Rail_Siding"]
    # main runs north (high y) -> south; cut it at the sample closest to the siding's first point
    if main[0].y < main[-1].y:
        main = list(reversed(main))
    if siding[0].y < siding[-1].y:
        siding = list(reversed(siding))
    s0 = siding[0]
    cut = min(range(len(main)), key=lambda i: (main[i].x - s0.x) ** 2 + (main[i].y - s0.y) ** 2)
    gap = math.dist([main[cut].x, main[cut].y], [s0.x, s0.y])
    # Since the 2026-11-18 13:04 map save the siding is its own line from beyond the north edge (x 20875), not a
    # branch off the main line: then the path is the siding alone (junction_index 0).
    siding_only = gap > 300.0
    if siding_only:
        cut = 0
    path = [v3(p) for p in main[:cut]] + [v3(p) for p in siding]
    probe["junction"] = {"main_cut_index": cut, "main_point": v3(main[cut]), "siding_start": v3(s0),
                         "gap_cm": round(gap, 1), "siding_only": siding_only}
    for a in actors:
        lab = a.get_actor_label()
        if lab.startswith("REAR_Station_Train") or lab.startswith("REAR_Station_Buffer"):
            o, e = a.get_actor_bounds(False)
            mesh = ""
            c = a.get_component_by_class(unreal.StaticMeshComponent)
            if c and c.static_mesh:
                mesh = c.static_mesh.get_path_name()
            probe["parked"].append({"label": lab, "loc": v3(a.get_actor_location()),
                                    "yaw": round(a.get_actor_rotation().yaw, 1), "mesh": mesh,
                                    "bounds_min": v3(o - e), "bounds_max": v3(o + e)})
        if "Blocker" in lab or lab.startswith("REAR_Skirt") or lab.startswith("REAR_Edge"):
            o, e = a.get_actor_bounds(False)
            probe["edges"].append({"label": lab, "min": v3(o - e), "max": v3(o + e)})
    for path_name in MESHES:
        probe["meshes"].append(mesh_info(path_name))
    spawn = (19953.0, 15593.0)
    probe["unit_spawn_ground"] = ground_z(w, *spawn)
    probe["path_start_ground"] = ground_z(w, path[0][0], path[0][1])
    with open(os.path.join(HERE, "train_probe.json"), "w", encoding="utf-8") as fh:
        json.dump(probe, fh, indent=1)
    with open(os.path.join(HERE, "train_path.json"), "w", encoding="utf-8") as fh:
        json.dump({"source": ("Ger_IntroMission REAR_Rail_Siding only (own line from beyond the north edge)"
                              if siding_only else
                              "Ger_IntroMission REAR_Rail_Main (north end -> siding junction) + REAR_Rail_Siding")
                             + ", sampled every ~%d cm, world space" % SAMPLE_STEP,
                   "junction_index": cut, "siding_only": siding_only,
                   "stop_front_location": STOP_FRONT_LOCATION, "points": path}, fh, indent=0)
    unreal.log("TRAINPROBE done: %d path points, junction %d" % (len(path), cut))


def tick(_dt):
    if STATE["done"]:
        return
    STATE["ticks"] += 1
    w = world()
    if w is None or MAP_NAME not in w.get_name():
        if STATE["ticks"] > 3000:
            unreal.log_error("TRAINPROBE: map never loaded (%s)" % (w.get_name() if w else None))
            STATE["done"] = True
            unreal.SystemLibrary.quit_editor()
        return
    if STATE["ticks"] < 30:
        return
    STATE["done"] = True
    try:
        run()
    except Exception:
        unreal.log_error("TRAINPROBE failed:\n" + traceback.format_exc())
    unreal.unregister_slate_post_tick_callback(STATE["handle"])
    unreal.SystemLibrary.quit_editor()


if __name__ == "train_cmd":
    # exec'd by train_editor_server.py: run now, keep the editor open
    run()
else:
    STATE["handle"] = unreal.register_slate_post_tick_callback(tick)
