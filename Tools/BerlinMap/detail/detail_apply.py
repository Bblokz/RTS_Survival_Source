"""Applies Saved/Berlin/detail/plan/placements.json (detail_gen.py) to Ger_Berlin in the throwaway editor.
Nothing is saved here; save through berlin_cmd with a nav rebuild afterwards.

    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/detail")
    import detail_apply as da; importlib.reload(da)
    da.clear(); da.apply()            # or apply(kinds=("decal",)) etc.

Labels: DET_D_<group>_<n> (decals), DET_B_<group>_<n> (Blueprints), DET_I_<mesh>[_C] (one HISM actor per mesh;
_C = with collision). Folders under Berlin/Detail. Seating: landscape trace (props ignored) at the pivot; Blueprints
and colliding instances use the lowest landscape point under their footprint (so nothing floats on a slope) minus 2 cm.
"""
import json
import math

import unreal

PLAN = "D:/UE5Projects/RTS_Survival/Saved/Berlin/detail/plan/placements.json"
PREFIX = "DET_"
_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
_cache = {}


def asset(path):
    if path not in _cache:
        _cache[path] = unreal.load_asset(path)
    return _cache[path]


def bp_class(path):
    key = "cls:" + path
    if key not in _cache:
        _cache[key] = unreal.EditorAssetLibrary.load_blueprint_class(path)
    return _cache[key]


def land_z(x, y):
    ignore = []
    for _ in range(16):
        hit = unreal.SystemLibrary.line_trace_single(_world, unreal.Vector(x, y, 40000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        t = hit.to_tuple()
        if "Landscape" in t[9].get_class().get_name():
            return t[4].z
        ignore.append(t[9])
    return None


def low_z(x, y, yaw, hx, hy):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    zs = []
    for fx, fy in ((0, 0), (1, 1), (1, -1), (-1, 1), (-1, -1), (1, 0), (-1, 0), (0, 1), (0, -1)):
        lx, ly = fx * hx * 0.85, fy * hy * 0.85
        z = land_z(x + lx * c - ly * s, y + lx * s + ly * c)
        if z is not None:
            zs.append(z)
    return (min(zs) - 2.0) if zs else None


def clear(prefix=PREFIX):
    n = 0
    for a in _sub.get_all_level_actors():
        if a.get_actor_label().startswith(prefix):
            _sub.destroy_actor(a)
            n += 1
    print("cleared", n)
    return n


def _instance_actor(label, folder, mesh, transforms, collision):
    actor = _sub.spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    comp.set_static_mesh(mesh)
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    # small clutter is culled at a distance the RTS camera never needs (perf); big pieces keep the default
    b = mesh.get_bounding_box()
    big = max(b.max.x - b.min.x, b.max.y - b.min.y) > 400
    comp.set_editor_property("instance_end_cull_distance", 0 if big else 16000)
    comp.set_editor_property("instance_start_cull_distance", 0 if big else 13000)
    comp.set_editor_property("cast_shadow", True)
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
    comp.add_instances(transforms, False, True)
    return actor


def apply(kinds=("decal", "bp", "ism")):
    P = json.load(open(PLAN))["placements"]
    stats = {"decal": 0, "bp": 0, "ism": 0, "no_ground": 0, "missing": 0}
    counters = {}
    ism = {}
    for p in P:
        k = p["kind"]
        if k not in kinds:
            continue
        if k == "decal":
            z = land_z(p["x"], p["y"])
            if z is None:
                stats["no_ground"] += 1
                continue
            mat = asset(p["mat"])
            if mat is None:
                stats["missing"] += 1
                continue
            a = _sub.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(p["x"], p["y"], z + 20.0),
                                            unreal.Rotator(roll=p["rot"], pitch=-90.0, yaw=0.0))
            d = a.decal
            d.set_decal_material(mat)
            d.set_editor_property("decal_size", unreal.Vector(float(p.get("depth", 160.0)), p["half"], p.get("half2", p["half"])))
            d.set_editor_property("fade_screen_size", 0.004)
            d.set_editor_property("sort_order", int(p.get("sort", 3)))
            if p.get("color"):
                d.set_editor_property("decal_color", unreal.LinearColor(*p["color"]))
            n = counters.setdefault(p["group"], 0) + 1
            counters[p["group"]] = n
            a.set_actor_label("DET_D_%s_%04d" % (p["group"], n))
            a.set_folder_path("Berlin/Detail/Decals/" + p["group"])
            stats["decal"] += 1
        elif k == "bp":
            cls = bp_class(p["path"])
            if cls is None:
                stats["missing"] += 1
                continue
            z = low_z(p["x"], p["y"], p["yaw"], 120, 120)
            if z is None:
                stats["no_ground"] += 1
                continue
            a = _sub.spawn_actor_from_class(cls, unreal.Vector(p["x"], p["y"], z), unreal.Rotator(0.0, 0.0, p["yaw"]))
            if a is None:
                stats["missing"] += 1
                continue
            if p.get("scale"):
                a.set_actor_scale3d(unreal.Vector(p["scale"], p["scale"], p["scale"]))
            if p.get("material"):
                m = asset(p["material"])
                for c in a.get_components_by_class(unreal.StaticMeshComponent):
                    for si in range(c.get_num_materials()):
                        c.set_material(si, m)
            if p.get("paint"):
                # BP_Cover_OldCar: the construction script writes the paint into primitive data 8-11 from these
                try:
                    a.set_editor_property("RandomPaint", False)
                    a.set_editor_property("Paint", unreal.LinearColor(p["paint"][0], p["paint"][1], p["paint"][2], 1.0))
                    a.set_editor_property("Wear", float(p.get("wear", 1.0)))
                except Exception as e:  # noqa: BLE001
                    print("car paint", e)
            n = counters.setdefault(p["group"], 0) + 1
            counters[p["group"]] = n
            a.set_actor_label("DET_B_%s_%04d" % (p["group"], n))
            a.set_folder_path("Berlin/Detail/Props/" + p["group"])
            stats["bp"] += 1
        elif k == "ism":
            mesh = asset(p["path"])
            if mesh is None:
                stats["missing"] += 1
                continue
            b = mesh.get_bounding_box()
            if p["collide"]:
                z = low_z(p["x"], p["y"], p["yaw"], (b.max.x - b.min.x) * 0.5 * p["scale"], (b.max.y - b.min.y) * 0.5 * p["scale"])
            else:
                z = land_z(p["x"], p["y"])
            if z is None:
                stats["no_ground"] += 1
                continue
            if p.get("pitch"):
                # pitch 90 lays the piece on its side: local X becomes vertical, so rest on the X minimum
                z = z - p.get("sink", 0.0) - b.min.x * p["scale"]
            else:
                z = z - p.get("sink", 0.0) - b.min.z * p["scale"]
            tr = unreal.Transform(unreal.Vector(p["x"], p["y"], z),
                                  unreal.Rotator(roll=float(p.get("roll", 0.0)), pitch=float(p.get("pitch", 0.0)), yaw=p["yaw"]),
                                  unreal.Vector(p["scale"], p["scale"], p["scale"]))
            ism.setdefault((p["mesh"], p["path"], p["collide"], tuple(p.get("cpd") or ())), []).append(tr)
            stats["ism"] += 1
    for (name, path, collide, cpd), trs in ism.items():
        act = _instance_actor("DET_I_%s%s" % (name, "_C" if collide else ""), "Berlin/Detail/Instanced", asset(path), trs, collide)
        if cpd:
            comp = act.get_component_by_class(unreal.InstancedStaticMeshComponent)
            for idx, val in zip(cpd[0::2], cpd[1::2]):
                comp.set_default_custom_primitive_data_float(int(idx), float(val))
    print("applied", stats, "groups", counters, "ism actors", len(ism))
    return stats


OLDCAR_MESH = "/Game/RTS_Survival/Blueprints/Environment/Props/OldCars/Meshes/SM_OldCar_%s_Wreck"
POSTAPO_TO_OLD = {1: "Sedan", 2: "Limousine", 3: "Coupe", 4: "Compact", 5: "PanelVan"}
UNDO_CARS = "D:/UE5Projects/RTS_Survival/Saved/Berlin/detail/postapo_cars_undo.json"


def convert_postapo_cars():
    """Round 2: the dress generator's colourful post-apocalyptic cars (BER_ISM_SM_PostApo_Car_1..5) become burnt 1940s
    wrecks (M_OldCar: primitive data 8-10 paint, 11 wear, 12 burnt). Long axis Y -> X, so each instance turns 90 deg.
    The original mesh and transforms go to postapo_cars_undo.json first."""
    import os
    undo = json.load(open(UNDO_CARS)) if os.path.exists(UNDO_CARS) else {}
    n = 0
    for a in _sub.get_all_level_actors():
        lab = a.get_actor_label()
        if not lab.startswith("BER_ISM_SM_PostApo_Car_"):
            continue
        k = int(lab.split("_")[-1])
        comp = a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        if comp.static_mesh.get_name().startswith("SM_OldCar"):
            continue                                            # already converted
        trs = [comp.get_instance_transform(i, True) for i in range(comp.get_instance_count())]
        undo[lab] = {"mesh": comp.static_mesh.get_path_name(),
                     "transforms": [[t.translation.x, t.translation.y, t.translation.z, t.rotation.rotator().roll,
                                     t.rotation.rotator().pitch, t.rotation.rotator().yaw, t.scale3d.x] for t in trs]}
        a.modify()
        comp.set_static_mesh(asset(OLDCAR_MESH % POSTAPO_TO_OLD.get(k, "Sedan")))
        for i, t in enumerate(trs):
            r = t.rotation.rotator()
            nt = unreal.Transform(t.translation, unreal.Rotator(roll=r.roll, pitch=r.pitch, yaw=r.yaw + 90.0), t.scale3d)
            comp.update_instance_transform(i, nt, True, False, True)
        for idx, v in ((8, 0.08), (9, 0.065), (10, 0.055), (11, 1.0), (12, 1.0)):
            comp.set_default_custom_primitive_data_float(idx, v)
        n += comp.get_instance_count()
    json.dump(undo, open(UNDO_CARS, "w"), indent=0)
    print("converted post-apo car instances", n)


DELETED_RUBDECALS = ["RUBDECAL_14_SandstoneBlocks_0008", "RUBDECAL_14_SandstoneBlocks_0016", "RUBDECAL_04_PlasterDust_0017",
                     "RUBDECAL_15_GritSpill_0018", "RUBDECAL_15_GritSpill_0019", "RUBDECAL_05_ConcreteRebar_0020",
                     "RUBDECAL_05_ConcreteRebar_0021", "RUBDECAL_15_GritSpill_0022", "RUBDECAL_14_SandstoneBlocks_0023"]


def remove_koenigsplatz_rubdecals():
    """Round 3: nine rubble decals of the 2026-11-17 pass read as pale hexagon stamps beside the Reichstag front."""
    n = 0
    for a in _sub.get_all_level_actors():
        if a.get_actor_label() in DELETED_RUBDECALS:
            _sub.destroy_actor(a)
            n += 1
    print("removed rubdecals", n)
