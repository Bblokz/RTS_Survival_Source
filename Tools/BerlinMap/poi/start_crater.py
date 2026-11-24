"""Ger_Berlin start crater, owner's pick 2026-11-19: concept v02 option 2 look B "Mauerreste"
(Docs/Concepts/BerlinStartCrater/v02). Editor steps, sent to crater_editor_server.py one at a time:

    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi")
    import start_crater as sc; importlib.reload(sc)
    sc.import_kit(); sc.build_bp()             # assets: 3 rim meshes + BP_BerlinStartCraterRim (saved)
    sc.record_old()                            # ground under every piece/decal before the dip (traces)
    sc.import_region()                         # landscape dip + paint (crater_heights.py output)
    sc.reseat(); sc.place()                    # crystals + decals down to the new ground, Crater0 rim hidden, rim placed

Rim kit: Blender generator bsc_kit.py in CustomMapItems/Environment/BerlinStartCrater (fitted to the new ground, pivot
at the crater centre on world z 0, so the Blueprint goes to (17300, 47300, 0) with no rotation). The rim meshes block
shells and sight but not navigation; hidden NavArea_Null boxes (one per heap = the concept obstacle, one per wall stub)
keep harvesters to the three breaches, so the nav numbers match the concept check.
Labels CRATER_* in the outliner folder Berlin/StartCrater; place() clears its own prefix first.
"""
import json
import math

import unreal

KIT = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinStartCrater/Kit/"
LAND = "D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/land/"
HOME = "/Game/RTS_Survival/Blueprints/Environment/Berlin/StartCrater"
BP_NAME = "BP_BerlinStartCraterRim"
MESHES = ("SM_BerlinCrater_Rim_East", "SM_BerlinCrater_Rim_South", "SM_BerlinCrater_Rim_West")
MATS = ("/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/MI_BerlinRuin_Wall",
        "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/MI_BerlinRuin_Rubble_Foot")
CENTRE = (17300.0, 47300.0)
PREFIX = "CRATER_"
FOLDER = "Berlin/StartCrater"
# start pieces: label -> footprint half size (cm, before yaw) for the seat traces
PIECES = {"RES_Start_Radixite_Crater": (500, 500), "RES_Start_Radixite_Large": (300, 420),
          "RES_Start_Radixite_Point": (120, 200), "RES_Start_Radixite_Crystal_1": (200, 220),
          "RES_Start_Radixite_Crystal_2": (200, 220), "RES_Start_Radixite_Crystal_3": (200, 220),
          "RES_Start_Radixite_Small_1": (110, 160), "RES_Start_Radixite_Small_2": (110, 160)}
RUIN_CPD = (0.9, 0.86, 0.82, 1.25, -1.0)   # slightly warm brick tint, heavy soot, rubble keeps its colour
SEAT_SINK = 10.0         # cm below the lowest new ground under the footprint (upright on the ~4 deg floor)
REGION = (14400.0, 20200.0, 44200.0, 49200.0)
EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary


def _sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _actors(prefix):
    return [a for a in _sub().get_all_level_actors() if a.get_actor_label().startswith(prefix)]


def _actor(label):
    found = [a for a in _sub().get_all_level_actors() if a.get_actor_label() == label]
    assert len(found) == 1, (label, len(found))
    return found[0]


def _landscape():
    return [a for a in _sub().get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)][0]


# ------------------------------------------------------------------ kit
def import_kit():
    """Legacy FBX import (Interchange drops UCX hulls). Checks the bounds against kit.json (catches a Y-mirror slip)."""
    kit = json.load(open(KIT + "kit.json"))
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        for name in MESHES:
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
            data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)  # soot/plaster/dust
            existing = unreal.load_asset(HOME + "/" + name) if EAL.does_asset_exist(HOME + "/" + name) else None
            if existing is not None:      # a replace-import reuses the asset's stored import data, not the new options
                aid = existing.get_editor_property("asset_import_data")
                aid.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
                aid.set_editor_property("auto_generate_collision", False)
                aid.set_editor_property("one_convex_hull_per_ucx", True)
                aid.set_editor_property("generate_lightmap_u_vs", False)
            task = unreal.AssetImportTask()
            task.filename = KIT + "FBX/" + name + ".fbx"
            task.destination_path = HOME
            task.destination_name = name
            task.automated = True
            task.save = False
            task.replace_existing = True
            task.options = ui
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")
    for name in MESHES:
        mesh = unreal.load_asset(HOME + "/" + name)
        assert mesh is not None, "import made nothing for " + name
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, unreal.load_asset(MATS[min(i, 1)]))
        mesh.set_editor_property("light_map_coordinate_index", 1)
        mesh.modify()
        assert unreal.StaticMeshEditorSubsystem and unreal.get_editor_subsystem(
            unreal.StaticMeshEditorSubsystem).has_vertex_colors(mesh), "vertex colours missing on " + name
        unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)
        box = mesh.get_bounding_box()
        body = mesh.get_editor_property("body_setup")
        hulls = len(body.get_editor_property("agg_geom").get_editor_property("convex_elems")) if body else 0
        lo, hi = kit["meshes"][name]["bounds_m_unreal"]
        err = max(abs(box.min.x - lo[0] * 100), abs(box.max.x - hi[0] * 100), abs(box.min.y - lo[1] * 100),
                  abs(box.max.y - hi[1] * 100), abs(box.min.z - lo[2] * 100), abs(box.max.z - hi[2] * 100))
        print("%s hulls %d (kit %d) bounds (%.0f %.0f %.0f)..(%.0f %.0f %.0f) vs kit: max err %.1f cm, materials %s" % (
            name, hulls, kit["meshes"][name]["hulls"], box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z,
            err, [mesh.get_material(i).get_name() for i in range(len(mesh.get_editor_property("static_materials")))]))
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


def build_bp():
    """BP_BerlinStartCraterRim: Actor with the 3 rim meshes and the NavArea_Null boxes from kit.json. Rebuilt in place."""
    kit = json.load(open(KIT + "kit.json"))
    path = HOME + "/" + BP_NAME
    if EAL.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.Actor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, HOME, unreal.Blueprint, factory)
    named = _named(bp)
    old = [h for n, (h, d) in named.items() if not n.startswith("__") and n != "DefaultSceneRoot"
           and not SDL.is_root_component(d)]
    if old:
        SDS.delete_subobjects(named["__actor__"][0], old, bp)
    named = _named(bp)
    root = named["__root__"][0]
    ch = unreal.CollisionChannel
    nboxes = 0
    for name in MESHES:
        comp = _add(bp, root, unreal.StaticMeshComponent, name.replace("SM_BerlinCrater_", ""))
        comp.set_editor_property("static_mesh", unreal.load_asset(HOME + "/" + name))
        comp.set_editor_property("can_ever_affect_navigation", False)
        comp.set_editor_property("receives_decals", False)
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        # ruin material custom primitive data: 0-2 tint, 3 soot amount, 4 rubble desaturation (BerlinRuinKitMaterial.md)
        for index, value in enumerate(RUIN_CPD):
            comp.set_default_custom_primitive_data_float(index, float(value))
        comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        comp.set_collision_object_type(ch.ECC_WORLD_STATIC)
        comp.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
        comp.set_collision_response_to_channel(ch.ECC_BUILDING_PLACEMENT, unreal.CollisionResponseType.ECR_OVERLAP)
        for blocked in (ch.ECC_VISIBILITY, ch.ECC_PROJECTILE, ch.ECC_ENEMY, ch.ECC_PLAYER):
            comp.set_collision_response_to_channel(blocked, unreal.CollisionResponseType.ECR_BLOCK)
        for box in kit["meshes"][name]["nav_boxes"]:
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
    print("BP", path, "meshes", len(MESHES), "nav boxes", nboxes, "saved", ok)


# ------------------------------------------------------------------ landscape
def _trace_land(x, y):
    """Landscape height at (x, y), ignoring every other actor."""
    ignore = []
    for _ in range(16):
        hit = unreal.SystemLibrary.line_trace_single(_world(), unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return None
        t = hit.to_tuple()
        if t[9] is not None and isinstance(t[9], unreal.LandscapeProxy):
            return t[4].z
        ignore.append(t[9])
    return None


def _footprint(x, y, yaw, half, n=5):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    out = []
    for i in range(n):
        for j in range(n):
            hx = half[0] * (2 * i / (n - 1) - 1)
            hy = half[1] * (2 * j / (n - 1) - 1)
            out.append((x + hx * c - hy * s, y + hx * s + hy * c))
    return out


def _decals():
    out = []
    for a in _sub().get_all_level_actors():
        if not isinstance(a, unreal.DecalActor):
            continue
        p = a.get_actor_location()
        if REGION[0] <= p.x <= REGION[1] and REGION[2] <= p.y <= REGION[3]:
            out.append(a)
    return out


def record_old():
    """Before the dip: lowest landscape under each start piece, landscape under each decal in the region."""
    rec = {"pieces": {}, "decals": {}}
    for label, half in PIECES.items():
        a = _actor(label)
        p = a.get_actor_location()
        zs = [_trace_land(x, y) for x, y in _footprint(p.x, p.y, a.get_actor_rotation().yaw, half)]
        rec["pieces"][label] = dict(loc=[p.x, p.y, p.z], ground_min=min(z for z in zs if z is not None))
    for a in _decals():
        p = a.get_actor_location()
        rec["decals"][a.get_actor_label()] = dict(loc=[p.x, p.y, p.z], ground=_trace_land(p.x, p.y))
    json.dump(rec, open(LAND + "old_seats.json", "w"), indent=1)
    print("recorded", len(rec["pieces"]), "pieces,", len(rec["decals"]), "decals")


def import_region():
    g = json.load(open(LAND + "grid.json"))
    land = _landscape()
    tl = land.get_editor_property("target_layers")
    names = [str(k) for k in tl.keys()]
    assert names == g["layers"], (names, g["layers"])
    infos = [tl[k].get_editor_property("layer_info_obj") for k in tl.keys()]
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(
        land, g["i0"], g["j0"], g["i1"], g["j1"], LAND + "new_height.r16", infos, [LAND + "new_%s.r8" % n for n in names])
    print("import_region", ok, (g["i0"], g["j0"], g["i1"], g["j1"]))
    assert ok
    for x, y in ((CENTRE[0], CENTRE[1]), (CENTRE[0] + 100, CENTRE[1] - 500), (16000, 45300), (17300, 49150)):
        print("  landscape at (%d, %d): %.0f cm" % (x, y, _trace_land(x, y)))


# ------------------------------------------------------------------ re-seat + rim
def reseat():
    rec = json.load(open(LAND + "old_seats.json"))
    for label, half in PIECES.items():
        a = _actor(label)
        p = a.get_actor_location()
        old = rec["pieces"][label]
        zs = [_trace_land(x, y) for x, y in _footprint(p.x, p.y, a.get_actor_rotation().yaw, half)]
        new_min = min(z for z in zs if z is not None)
        z = old["loc"][2] + (new_min - old["ground_min"]) - SEAT_SINK
        a.modify()
        a.set_actor_location(unreal.Vector(p.x, p.y, z), False, False)          # never re-run construction: re-rolls
        print("seat %-30s z %6.0f -> %6.0f (new ground %.0f..%.0f)" % (label, old["loc"][2], z, new_min, max(zs)))
    crater = _actor("RES_Start_Radixite_Crater")
    mesh = [c for c in crater.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "MainMesh"][0]
    crater.modify()
    mesh.modify()
    mesh.set_visibility(False)
    mesh.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    mesh.set_editor_property("can_ever_affect_navigation", False)
    print("Crater0 rim (MainMesh) hidden, no collision, no nav")
    moved = 0
    for label, d in rec["decals"].items():
        a = _actor(label)
        if d["ground"] is None:
            continue
        g = _trace_land(d["loc"][0], d["loc"][1])
        dz = g - d["ground"]
        if abs(dz) < 3:
            continue
        a.modify()
        a.set_actor_location(unreal.Vector(d["loc"][0], d["loc"][1], d["loc"][2] + dz), False, False)
        moved += 1
        print("decal %-26s dz %+.0f" % (label, dz))
    print("decals moved", moved)


def place():
    for a in _actors(PREFIX):
        _sub().destroy_actor(a)
    cls = unreal.load_asset(HOME + "/" + BP_NAME).generated_class()
    a = _sub().spawn_actor_from_class(cls, unreal.Vector(CENTRE[0], CENTRE[1], 0.0), unreal.Rotator(0, 0, 0))
    a.set_actor_label(PREFIX + "Rim")
    a.set_folder_path(FOLDER)
    boxes = [c for c in a.get_components_by_class(unreal.BoxComponent)]
    meshes = [c for c in a.get_components_by_class(unreal.StaticMeshComponent)]
    print("placed", a.get_actor_label(), "meshes", len(meshes), "nav boxes", len(boxes))
