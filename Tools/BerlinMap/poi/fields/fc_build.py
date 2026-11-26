"""Ger_Berlin field craters E1/E2/E5/E6 (owner: "Yes, build them", 2026-11-26; concept sheets
Docs/Concepts/BerlinFieldCraters/v01, design + kit tools in CustomMapItems/Environment/BerlinFieldCraters).
Editor steps, sent to crater_editor_server.py:

    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/fields")
    import fc_build as fb; importlib.reload(fb)
    fb.survey(k)                      # dry run: what the dish would move (no changes)
    fb.import_kit(k); fb.build_bp(k)  # assets (saved): SM_BerlinFieldCrater_<k>_Rim_<n> + BP_BerlinFieldCrater_<k>
    fb.record_old(k)                  # absolute z of everything to re-seat, written ONCE (fields/<k>/old_seats.json)
    fb.import_region(k)               # landscape dish + paint (bfc_build.py output)
    fb.reseat(k); fb.place(k)         # crystals / props / decals / instances down by the dish, Crater0 rim hidden, rim BP

Labels FCRATER_<k>_* in the outliner folder Berlin/FieldCraters (place() clears only its own label).
Rim meshes block shells and sight but not navigation; hidden NavArea_Null boxes (heap ellipse * 0.9 as the equal-area
rectangle, stub footprint + 30 cm) keep the harvesters off the rim, like the start crater.
"""
import array
import json
import math
import os

import unreal

FIELDS = "D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/fields/"
KITS = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinFieldCraters/Kit/"
HOME = "/Game/RTS_Survival/Blueprints/Environment/Berlin/FieldCraters"
MATS = ("/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/MI_BerlinRuin_Wall",
        "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/MI_BerlinRuin_Rubble_Foot")
RUIN_CPD = (0.9, 0.86, 0.82, 1.25, -1.0)
FOLDER = "Berlin/FieldCraters"
MOVE_MIN_CM = 2.0
# The start crater hid Crater0's MainMesh (its 1.5 m bowl replaced it). The field dishes are 0.3-0.6 m and level the core
# out to 5.2 m, so the crater mesh moves down with its ground and stays: on E2 it covers a 63 cm pit under the core.
HIDE_CRATER0_RIM = False
SKIP_CLS = ("Landscape", "Volume", "Strongpoint", "Spawner", "Route", "Mission", "Trigger", "Supply", "PlayerStart",
            "Recast", "Navigation", "WorldSettings", "Light", "Fog", "Sky", "PostProcess", "Brush", "Info", "Manager",
            "BP_BerlinFieldCrater")
EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary


def _sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _landscape():
    return [a for a in _sub().get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)][0]


def _rim(k):
    return json.load(open(FIELDS + k + "/rim.json"))


def _kit(k):
    return json.load(open(KITS + k + "/kit.json"))


def _bp_name(k):
    return "BP_BerlinFieldCrater_%s" % k


class Delta:
    """Landscape delta (cm) of a site, bilinear on the 1 m vertex grid; 0 outside."""

    def __init__(self, k):
        m = json.load(open(FIELDS + k + "/delta.json"))
        self.m = m
        self.a = array.array("f")
        self.a.frombytes(open(FIELDS + k + "/delta.bin", "rb").read())

    def at(self, x, y):
        m = self.m
        fi = (x - m["lx"]) / m["sx"] - m["i0"]
        fj = (y - m["ly"]) / m["sy"] - m["j0"]
        if fi < 0 or fj < 0 or fi > m["nx"] - 1.001 or fj > m["ny"] - 1.001:
            return 0.0
        i, j = int(fi), int(fj)
        a, b = fi - i, fj - j
        nx = m["nx"]
        v = self.a
        return (v[j * nx + i] * (1 - a) * (1 - b) + v[j * nx + i + 1] * a * (1 - b) + v[(j + 1) * nx + i] * (1 - a) * b
                + v[(j + 1) * nx + i + 1] * a * b)

    def core_min(self, x, y, r_cm):
        out = 0.0
        n = int(r_cm // 50)
        for di in range(-n, n + 1):
            for dj in range(-n, n + 1):
                if math.hypot(di * 50, dj * 50) <= r_cm:
                    out = min(out, self.at(x + di * 50, y + dj * 50))
        return out


# ------------------------------------------------------------------ assets
def import_kit(k):
    kit = _kit(k)
    names = list(kit["meshes"])
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        for name in names:
            ui = unreal.FbxImportUI()
            ui.set_editor_property("import_mesh", True)
            ui.set_editor_property("import_as_skeletal", False)
            ui.set_editor_property("import_materials", False)
            ui.set_editor_property("import_textures", False)
            ui.set_editor_property("import_animations", False)
            ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
            data = ui.get_editor_property("static_mesh_import_data")
            data.set_editor_property("combine_meshes", True)
            data.set_editor_property("auto_generate_collision", False)
            data.set_editor_property("one_convex_hull_per_ucx", True)
            data.set_editor_property("generate_lightmap_u_vs", False)
            data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
            data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
            existing = unreal.load_asset(HOME + "/" + name) if EAL.does_asset_exist(HOME + "/" + name) else None
            if existing is not None:
                aid = existing.get_editor_property("asset_import_data")
                aid.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
                aid.set_editor_property("auto_generate_collision", False)
                aid.set_editor_property("one_convex_hull_per_ucx", True)
                aid.set_editor_property("generate_lightmap_u_vs", False)
            task = unreal.AssetImportTask()
            task.filename = KITS + k + "/FBX/" + name + ".fbx"
            task.destination_path = HOME
            task.destination_name = name
            task.automated = True
            task.save = False
            task.replace_existing = True
            task.options = ui
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")
    for name in names:
        mesh = unreal.load_asset(HOME + "/" + name)
        assert mesh is not None, "import made nothing for " + name
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, unreal.load_asset(MATS[min(i, 1)]))
        mesh.set_editor_property("light_map_coordinate_index", 1)
        mesh.modify()
        assert unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).has_vertex_colors(mesh), "no vertex colours " + name
        unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)
        box = mesh.get_bounding_box()
        body = mesh.get_editor_property("body_setup")
        hulls = len(body.get_editor_property("agg_geom").get_editor_property("convex_elems")) if body else 0
        lo, hi = kit["meshes"][name]["bounds_m_unreal"]
        err = max(abs(box.min.x - lo[0] * 100), abs(box.max.x - hi[0] * 100), abs(box.min.y - lo[1] * 100),
                  abs(box.max.y - hi[1] * 100), abs(box.min.z - lo[2] * 100), abs(box.max.z - hi[2] * 100))
        print("%s hulls %d (kit %d) bounds err %.1f cm, materials %s" % (
            name, hulls, kit["meshes"][name]["hulls"], err,
            [mesh.get_material(i).get_name() for i in range(len(mesh.get_editor_property("static_materials")))]))
        assert err < 5.0, "bounds differ from the kit (mirror?)"
        assert hulls == kit["meshes"][name]["hulls"], "hull count differs"


def _named(bp):
    out = {}
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDL.get_data(handle)
        template = SDL.get_object(data)
        if template is None or isinstance(template, unreal.Actor):
            out.setdefault("__actor__", (handle, data))
            continue
        out[str(SDL.get_variable_name(data))] = (handle, data)
        if SDL.is_root_component(data):
            out["__root__"] = (handle, data)
    return out


def _add(bp, parent, cls, name):
    params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError("adding %s failed: %s" % (name, fail))
    SDS.rename_subobject(handle, unreal.Text(name))
    return SDL.get_object(SDL.get_data(handle))


def build_bp(k, skip_boxes=()):
    """skip_boxes: global nav box indices to leave out (diagnostics only)."""
    kit = _kit(k)
    name = _bp_name(k)
    path = HOME + "/" + name
    if EAL.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.Actor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, HOME, unreal.Blueprint, factory)
    named = _named(bp)
    old = [h for n, (h, d) in named.items() if not n.startswith("__") and n != "DefaultSceneRoot"
           and not SDL.is_root_component(d)]
    if old:
        SDS.delete_subobjects(named["__actor__"][0], old, bp)
    named = _named(bp)
    root = named["__root__"][0]
    ch = unreal.CollisionChannel
    nboxes = 0
    for mname, m in kit["meshes"].items():
        comp = _add(bp, root, unreal.StaticMeshComponent, mname.replace("SM_BerlinFieldCrater_%s_" % k, ""))
        comp.set_editor_property("static_mesh", unreal.load_asset(HOME + "/" + mname))
        comp.set_editor_property("can_ever_affect_navigation", False)
        comp.set_editor_property("receives_decals", False)
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        for index, value in enumerate(RUIN_CPD):
            comp.set_default_custom_primitive_data_float(index, float(value))
        comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        comp.set_collision_object_type(ch.ECC_WORLD_STATIC)
        comp.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
        comp.set_collision_response_to_channel(ch.ECC_BUILDING_PLACEMENT, unreal.CollisionResponseType.ECR_OVERLAP)
        for blocked in (ch.ECC_VISIBILITY, ch.ECC_PROJECTILE, ch.ECC_ENEMY, ch.ECC_PLAYER):
            comp.set_collision_response_to_channel(blocked, unreal.CollisionResponseType.ECR_BLOCK)
        for box in m["nav_boxes"]:
            if nboxes in skip_boxes:
                nboxes += 1
                continue
            b = _add(bp, root, unreal.BoxComponent, "NavBlock_%02d" % nboxes)
            b.set_editor_property("relative_location", unreal.Vector(box["cx"], box["cy"], (box["z0"] + box["z1"]) * 0.5))
            b.set_editor_property("relative_rotation", unreal.Rotator(roll=0, pitch=0, yaw=box["yaw"]))
            b.set_editor_property("box_extent", unreal.Vector(box["hu"], box["hv"], (box["z1"] - box["z0"]) * 0.5))
            b.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
            b.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            b.set_editor_property("generate_overlap_events", False)
            b.set_editor_property("can_ever_affect_navigation", True)
            b.set_editor_property("dynamic_obstacle", True)
            b.set_editor_property("area_class_override", unreal.NavArea_Null)
            b.set_editor_property("use_system_default_obstacle_area_class", False)
            b.set_editor_property("visible", False)
            b.set_editor_property("hidden_in_game", True)
            nboxes += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    ok = EAL.save_loaded_asset(bp, False)
    print("BP", path, "meshes", len(kit["meshes"]), "nav boxes", nboxes, "saved", ok)


# ------------------------------------------------------------------ what moves
def _skip(a):
    cn = a.get_class().get_name()
    if any(s in cn for s in SKIP_CLS):
        return True
    lab = a.get_actor_label()
    return lab.startswith("FCRATER_") or lab.startswith("CRATER_")


def _movers(k):
    """Actors whose pivot sits on the dish (|delta| >= 2 cm) or that are field pieces; ISM / HISM instances likewise."""
    rim = _rim(k)
    dl = Delta(k)
    x0, x1, y0, y1 = rim["region"]
    pieces = set(rim["seats"])
    acts, insts, big, foliage = [], [], [], []
    for a in _sub().get_all_level_actors():
        p = a.get_actor_location()
        if not (x0 <= p.x <= x1 and y0 <= p.y <= y1):
            continue
        lab = a.get_actor_label()
        if isinstance(a, unreal.InstancedFoliageActor):
            foliage.append(a)
        if _skip(a) and lab not in pieces:
            continue
        if lab in pieces:
            acts.append((a, "piece"))
            continue
        dz = dl.at(p.x, p.y)
        if abs(dz) >= MOVE_MIN_CM:
            o, e = a.get_actor_bounds(False)
            kind = "decal" if isinstance(a, unreal.DecalActor) else "actor"
            if kind == "actor" and (e.x > 150 or e.y > 150):
                big.append((lab, a.get_class().get_name(), round(e.x * 2 / 100, 1), round(e.y * 2 / 100, 1), round(dz, 1)))
            acts.append((a, kind))
    for a in _sub().get_all_level_actors():
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            n = c.get_instance_count()
            for i in range(n):
                t = c.get_instance_transform(i, True)
                q = t.translation
                if not (x0 <= q.x <= x1 and y0 <= q.y <= y1):
                    continue
                if abs(dl.at(q.x, q.y)) >= MOVE_MIN_CM:
                    insts.append((a, c, i))
    return acts, insts, big, foliage


def survey(k):
    acts, insts, big, foliage = _movers(k)
    from collections import Counter
    cnt = Counter(kind for _, kind in acts)
    cls = Counter(a.get_class().get_name() for a, kind in acts if kind == "actor")
    icnt = Counter("%s/%s" % (a.get_actor_label(), c.static_mesh.get_name() if c.static_mesh else None) for a, c, i in insts)
    print("%s movers: %s; actor classes %s" % (k, dict(cnt), dict(cls)))
    print("   instances on the dish:", dict(icnt))
    print("   foliage actors in region:", [f.get_actor_label() for f in foliage],
          "foliage instances on dish:", sum(1 for a, c, i in insts if isinstance(a, unreal.InstancedFoliageActor)))
    print("   bigger than 3 m (pivot shift only):", big)


def record_old(k):
    path = FIELDS + k + "/old_seats.json"
    if os.path.exists(path):
        print("old seats already recorded, kept:", path)
        return
    acts, insts, big, foliage = _movers(k)
    rec = {"actors": {}, "instances": []}
    for a, kind in acts:
        p = a.get_actor_location()
        rec["actors"][a.get_actor_label()] = dict(kind=kind, loc=[p.x, p.y, p.z], path=a.get_path_name())
    for a, c, i in insts:
        if isinstance(a, unreal.InstancedFoliageActor):
            continue                     # foliage instances: not touched (survey reports them)
        t = c.get_instance_transform(i, True)
        rec["instances"].append(dict(actor=a.get_actor_label(), comp=c.get_name(), i=i,
                                     loc=[t.translation.x, t.translation.y, t.translation.z]))
    json.dump(rec, open(path, "w"), indent=1)
    print("recorded", len(rec["actors"]), "actors,", len(rec["instances"]), "instances ->", path)


# ------------------------------------------------------------------ landscape
def import_region(k):
    g = json.load(open(FIELDS + k + "/grid.json"))
    land = _landscape()
    tl = land.get_editor_property("target_layers")
    names = [str(n) for n in tl.keys()]
    assert names == g["layers"], (names, g["layers"])
    infos = [tl[n].get_editor_property("layer_info_obj") for n in tl.keys()]
    d = FIELDS + k + "/"
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(
        land, g["i0"], g["j0"], g["i1"], g["j1"], d + "new_height.r16", infos, [d + "new_%s.r8" % n for n in names])
    print("import_region", k, ok, (g["i0"], g["j0"], g["i1"], g["j1"]))
    assert ok


def _land_z(x, y):
    w = _world()
    ignore = []
    for _ in range(16):
        hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        t = hit.to_tuple()
        if isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
        ignore.append(t[9])
    return None


# ------------------------------------------------------------------ re-seat + rim
def reseat(k):
    rim = _rim(k)
    dl = Delta(k)
    rec = json.load(open(FIELDS + k + "/old_seats.json"))
    by_label = {a.get_actor_label(): a for a in _sub().get_all_level_actors()}
    moved = {"piece": 0, "actor": 0, "decal": 0}
    for lab, r in rec["actors"].items():
        a = by_label.get(lab)
        if a is None:
            print("  missing", lab)
            continue
        x, y, z = r["loc"]
        if r["kind"] == "piece":
            rr = 520.0 if rim["seats"][lab]["cls"] == "Crater0_C" else 200.0
            dz = dl.core_min(x, y, rr)            # deepest point under the core: nothing floats
        else:
            dz = dl.at(x, y)
        a.modify()
        a.set_actor_location(unreal.Vector(x, y, z + dz), False, False)   # location only: no construction re-roll
        moved[r["kind"]] += 1
        if r["kind"] == "piece":
            print("  seat %-28s dz %+.1f cm" % (lab, dz))
    comps = {}
    for a in _sub().get_all_level_actors():
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            comps[(a.get_actor_label(), c.get_name())] = c
    ni = 0
    for r in rec["instances"]:
        c = comps.get((r["actor"], r["comp"]))
        if c is None:
            print("  missing instance comp", r["actor"], r["comp"])
            continue
        t = c.get_instance_transform(r["i"], True)
        x, y, z = r["loc"]
        if math.hypot(t.translation.x - x, t.translation.y - y) > 1.0:
            print("  instance moved since record, skipped", r)
            continue
        t.translation = unreal.Vector(x, y, z + dl.at(x, y))
        c.modify()
        c.update_instance_transform(r["i"], t, True, True, True)
        ni += 1
    print("moved", moved, "instances", ni)
    if rim["dip"] and HIDE_CRATER0_RIM:
        for lab in rim["seats"]:
            if rim["seats"][lab]["cls"] != "Crater0_C":
                continue
            a = by_label[lab]
            mesh = [c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "MainMesh"][0]
            a.modify()
            mesh.modify()
            mesh.set_visibility(False)
            mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            mesh.set_editor_property("can_ever_affect_navigation", False)
            print("  %s rim (MainMesh) hidden, no collision, no nav" % lab)


def place(k):
    label = "FCRATER_%s_Rim" % k
    for a in _sub().get_all_level_actors():
        if a.get_actor_label() == label:
            _sub().destroy_actor(a)
    rim = _rim(k)
    cls = unreal.load_asset(HOME + "/" + _bp_name(k)).generated_class()
    a = _sub().spawn_actor_from_class(cls, unreal.Vector(rim["pivot"][0], rim["pivot"][1], 0.0), unreal.Rotator(0, 0, 0))
    a.set_actor_label(label)
    a.set_folder_path(FOLDER)
    print("placed", label, "meshes", len(a.get_components_by_class(unreal.StaticMeshComponent)),
          "nav boxes", len(a.get_components_by_class(unreal.BoxComponent)))


def ground_check(k):
    """Landscape vs the planned new ground at the pieces (after import)."""
    rim = _rim(k)
    rec = json.load(open(FIELDS + k + "/old_seats.json"))
    for lab in rim["seats"]:
        x, y, z = rec["actors"][lab]["loc"]
        print("  %-28s landscape %.0f cm, piece z %.0f" % (lab, _land_z(x, y) or 0,
                                                            [a for a in _sub().get_all_level_actors()
                                                             if a.get_actor_label() == lab][0].get_actor_location().z))


def restore_crater0_rim(k):
    by_label = {a.get_actor_label(): a for a in _sub().get_all_level_actors()}
    for lab, r in _rim(k)["seats"].items():
        if r["cls"] != "Crater0_C":
            continue
        a = by_label[lab]
        mesh = [c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "MainMesh"][0]
        a.modify()
        mesh.modify()
        mesh.set_visibility(True)
        mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        mesh.set_editor_property("can_ever_affect_navigation", True)
        print("  %s MainMesh visible %s, collision %s, nav %s" % (lab, mesh.is_visible(), mesh.get_collision_enabled(),
                                                                mesh.get_editor_property("can_ever_affect_navigation")))
