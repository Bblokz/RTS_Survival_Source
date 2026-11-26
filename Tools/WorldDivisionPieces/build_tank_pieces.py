"""Builds the world map division tank pieces: one static mesh per Soviet tank, and one per wreck.

Run headless (no GPU):
  sh run_py.sh build_tank_pieces.py                  builds every tank of tank_config.py
  sh run_py.sh build_tank_pieces.py T34_76           builds only the T-34/76
  sh run_py.sh build_tank_pieces.py T34_76 ALL       builds the T-34/76 and only lists the parts of the others
Log (appended per run): Saved/WorldDivisionPieces/tank_build.txt
Sources per mesh:       Saved/WorldDivisionPieces/tank_sources.json (read by check_tank_pieces.py)

Method (same as IntroMission/counterattack/cargo_tank_bake.py and the airfield build site piece): the vehicle Blueprint
is spawned at the origin of the commandlet's empty world, every visible mesh component of the actor and of its child
actors (the turret) is copied in its reference pose with Geometry Script into one dynamic mesh, with the component's
own materials. The actor's own scale is kept (the T-26 Blueprint is scaled 1.1), so the piece has the in-game size. Over the triangle budget the merged mesh is simplified as a whole (the simplifier keeps
material borders and UV seams; box, material ids and UV boxes are compared before and after). The mesh is then moved
so the pivot is at the bottom centre of the hull footprint. Only new assets in tank_config.DEST are created and saved.

Wrecks: the Env_Dest_* Blueprints throw the turret off with a random pose in their construction script. The script
spawns the wreck WRECK_SPAWNS times and keeps the spawn whose turret lies most on the hull, so a rerun gives a wreck
with a slightly different turret pose.
"""
import json
import os
import sys
import time
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tank_config  # noqa: E402

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldDivisionPieces")
os.makedirs(OUT, exist_ok=True)
HANDLE = open(os.path.join(OUT, "tank_build.txt"), "a", encoding="utf-8")
SOURCES_PATH = os.path.join(OUT, "tank_sources.json")

TRIANGLE_BUDGET = 4000
SIMPLIFY_TARGET = 3940          # the static mesh build can split a few triangles; stay under the budget
BOUNDS_TOLERANCE = 5.0          # cm the box may move by simplifying (the tanks are 5 to 10 m long)
UV_TOLERANCE = 0.03
WRECK_SPAWNS = 8
PROFILE_BINS = 16
LIVE_PROFILES = {}              # tank name -> (x0, x1, y centre, y half width, height profile of hull and tracks)

assets = unreal.EditorAssetLibrary
gs_assets = unreal.GeometryScript_AssetUtils
gs_scene = unreal.GeometryScript_SceneUtils
gs_edits = unreal.GeometryScript_MeshEdits
gs_mat = unreal.GeometryScript_Materials
gs_query = unreal.GeometryScript_MeshQueries
gs_xform = unreal.GeometryScript_MeshTransforms
gs_simplify = unreal.GeometryScript_MeshSimplification
gs_select = unreal.GeometryScript_MeshSelection
gs_uvs = unreal.GeometryScript_UVs
gs_list = unreal.GeometryScript_List
gs_repair = unreal.GeometryScript_MeshRepair
SUCCESS = unreal.GeometryScriptOutcomePins.SUCCESS


def log(text):
    HANDLE.write(str(text) + "\n")
    HANDLE.flush()
    unreal.log("DIVTANK " + str(text)[:250])


def vec(v):
    return "(%.1f %.1f %.1f)" % (v.x, v.y, v.z)


def rot(r):
    return "(p %.1f y %.1f r %.1f)" % (r.pitch, r.yaw, r.roll)


def short(obj):
    return obj.get_path_name().split(".")[0] if obj is not None else "None"


# ---------------------------------------------------------------------------------------------------------------------
# Dynamic mesh helpers
# ---------------------------------------------------------------------------------------------------------------------

def max_material_id(mesh):
    result = gs_mat.get_max_material_id(mesh)
    return result[0] if isinstance(result, tuple) else result


def triangles_per_material(mesh):
    """{material id: triangle count}."""
    result = gs_mat.get_all_triangle_material_i_ds(mesh)
    items = result if isinstance(result, tuple) else (result,)
    for item in items:
        if isinstance(item, unreal.GeometryScriptIndexList):
            counts = {}
            for material_id in gs_list.convert_index_list_to_array(item):
                counts[material_id] = counts.get(material_id, 0) + 1
            return counts
    return {}


def uv_bounds(mesh, material_id):
    """UV channel 0 box (min u, min v, max u, max v) of the triangles with this material id, or None."""
    try:
        selection = gs_select.select_mesh_elements_by_material_id(
            mesh, material_id, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
        if isinstance(selection, tuple):
            selection = [item for item in selection if isinstance(item, unreal.GeometryScriptMeshSelection)][-1]
        result = gs_uvs.get_mesh_uv_size_info(mesh, 0, selection)
        for item in result if isinstance(result, tuple) else (result,):
            if isinstance(item, unreal.Box2D):
                return (item.min.x, item.min.y, item.max.x, item.max.y)
    except Exception as error:
        log("    uv bounds unavailable: %s" % error)
    return None


def box_of(mesh):
    return gs_query.get_mesh_bounding_box(mesh)


def union(boxes):
    low = [min(getattr(box.min, axis) for box in boxes) for axis in "xyz"]
    high = [max(getattr(box.max, axis) for box in boxes) for axis in "xyz"]
    return low, high


# ---------------------------------------------------------------------------------------------------------------------
# Gathering the parts of a spawned Blueprint
# ---------------------------------------------------------------------------------------------------------------------

def component_mesh(component):
    if isinstance(component, unreal.StaticMeshComponent):
        return component.get_editor_property("static_mesh")
    if isinstance(component, unreal.SkinnedMeshComponent):
        return component.get_editor_property("skinned_asset")
    return None


def all_actors(actor, chain, seen, out):
    if actor is None or actor in seen:
        return
    seen.add(actor)
    out.append((actor, chain))
    for component in actor.get_components_by_class(unreal.ChildActorComponent):
        all_actors(component.get_editor_property("child_actor"), chain + [component.get_name()], seen, out)
    for attached in actor.get_attached_actors():
        all_actors(attached, chain + ["attached"], seen, out)


def skip_reason(component, mesh):
    if not component.is_visible():
        return "not visible"
    if component.get_editor_property("hidden_in_game"):
        return "hidden in game"
    name = mesh.get_name().lower()
    if "collision" in name or "collilsion" in name:
        return "collision mesh"
    return None


def copy_component(component, mesh, label, options):
    """Component -> dynamic mesh in world space, or None."""
    dynamic = unreal.DynamicMesh()
    result = gs_scene.copy_mesh_from_component(component, dynamic, options, True)
    outcome = result[-1] if isinstance(result, tuple) else result
    if (outcome != SUCCESS or dynamic.get_triangle_count() == 0) and isinstance(mesh, unreal.SkeletalMesh):
        # The component could not be posed (seen on the T-26): take the asset's reference pose instead.
        dynamic = unreal.DynamicMesh()
        result = gs_assets.copy_mesh_from_skeletal_mesh(
            mesh, dynamic, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        outcome = result[-1] if isinstance(result, tuple) else result
        gs_xform.transform_mesh(dynamic, component.get_world_transform())
        log("  note %s: the component copy failed, used the reference pose of the asset (%s)" % (label, outcome))
    if outcome != SUCCESS or dynamic.get_triangle_count() == 0:
        log("  skip %s (copy failed: %s)" % (label, outcome))
        return None
    return dynamic


def gather_parts(blueprint_path, quiet=False):
    """Spawns the Blueprint at the origin and returns its visible mesh parts in actor space (the actor is destroyed)."""
    actors_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls = assets.load_blueprint_class(blueprint_path)
    if cls is None:
        log("  no Blueprint class at " + blueprint_path)
        return []
    actor = actors_subsystem.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        log("  spawn failed " + blueprint_path)
        return []
    parts = []
    found = []
    try:
        # The actor stands at the origin without rotation, so world space is actor space with the actor's scale.
        scale = actor.get_actor_scale3d()
        if not quiet and (abs(scale.x - 1.0) > 0.001 or abs(scale.y - 1.0) > 0.001 or abs(scale.z - 1.0) > 0.001):
            log("  note: the actor is scaled %s; the piece keeps that scale" % vec(scale))
        all_actors(actor, [], set(), found)
        options = unreal.GeometryScriptCopyMeshFromComponentOptions()
        options.set_editor_property("want_normals", True)
        options.set_editor_property("want_tangents", True)
        for owner, _chain in found:
            for component in owner.get_components_by_class(unreal.MeshComponent):
                mesh = component_mesh(component)
                if mesh is None:
                    continue
                label = "%s.%s" % (owner.get_class().get_name(), component.get_name())
                reason = skip_reason(component, mesh)
                if reason is not None:
                    if not quiet:
                        log("  skip %s (%s) mesh %s" % (label, reason, mesh.get_name()))
                    continue
                dynamic = copy_component(component, mesh, label, options)
                if dynamic is None:
                    continue
                # Baked hulls (T-26) carry vertices without triangles; they would widen the boxes.
                gs_repair.remove_unused_vertices(dynamic)
                relative = component.get_world_transform()
                box = box_of(dynamic)
                part = {
                    "label": label, "component": component.get_name(), "on_root_actor": owner == actor,
                    "mesh": dynamic, "source": short(mesh), "materials": list(component.get_materials()),
                    "triangles": dynamic.get_triangle_count(), "box": box, "pivot_x": relative.translation.x,
                }
                part["line"] = "  part %s %s tris %d box x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f loc %s rot %s mats %s" % (
                    label, mesh.get_name(), part["triangles"], box.min.x, box.max.x, box.min.y, box.max.y,
                    box.min.z, box.max.z, vec(relative.translation), rot(relative.rotation.rotator()),
                    [m.get_name() if m else None for m in part["materials"]])
                parts.append(part)
                if not quiet:
                    log(part["line"])
    finally:
        for owner, _chain in reversed(found):
            try:
                actors_subsystem.destroy_actor(owner)
            except Exception:
                pass
    return parts


def footprint_parts(parts, is_wreck):
    """The parts that stand on the ground: the vehicle actor's own parts (not the turret child actor); for a wreck the
    hull (the thrown-off turret lies on or beside it)."""
    if is_wreck:
        hulls = [part for part in parts if "hull" in part["component"].lower()]
        return hulls or [max(parts, key=lambda entry: entry["triangles"])]
    roots = [part for part in parts if part["on_root_actor"]]
    return roots or parts


def wreck_score(parts):
    """Lower is better: how far the thrown-off turret hangs past the hull box, and how deep it sinks under it."""
    hulls = footprint_parts(parts, True)
    low, high = union([part["box"] for part in hulls])
    score = 0.0
    for part in parts:
        if part in hulls:
            continue
        box = part["box"]
        score += max(0.0, low[0] - box.min.x) + max(0.0, box.max.x - high[0])
        score += max(0.0, low[1] - box.min.y) + max(0.0, box.max.y - high[1])
        score += 10.0 * max(0.0, low[2] - box.min.z)
    return score


def gather_wreck(blueprint_path):
    best = None
    scores = []
    for _index in range(WRECK_SPAWNS):
        parts = gather_parts(blueprint_path, quiet=True)
        if not parts:
            return []
        score = wreck_score(parts)
        scores.append(score)
        if best is None or score < best[0]:
            best = (score, parts)
    log("  wreck spawned %d times, turret-off-hull scores %s; kept the spawn with %.0f" % (
        WRECK_SPAWNS, ["%.0f" % value for value in scores], best[0]))
    for part in best[1]:
        log(part["line"])
    return best[1]


# ---------------------------------------------------------------------------------------------------------------------
# Merge, triangle budget, front axis
# ---------------------------------------------------------------------------------------------------------------------

def merge(parts):
    """All parts in one mesh, one material id per different material. Returns (mesh, materials by id)."""
    target = unreal.DynamicMesh()
    materials = []
    identity = unreal.Transform()
    for part in parts:
        mesh = part["mesh"]
        offset = len(materials)
        top = max(max_material_id(mesh), len(part["materials"]) - 1)
        for material_id in range(top, -1, -1):
            gs_mat.remap_material_i_ds(mesh, material_id, material_id + offset)
        slots = list(part["materials"])
        while len(slots) < top + 1:
            slots.append(None)
        materials.extend(slots)
        gs_edits.append_mesh(target, mesh, identity, False, unreal.GeometryScriptAppendMeshOptions())
    used = triangles_per_material(target)
    unique = []
    for index, material in enumerate(materials):
        if used.get(index, 0) > 0 and material not in unique:
            unique.append(material)
    for index, material in enumerate(materials):
        if used.get(index, 0) > 0:
            gs_mat.remap_material_i_ds(target, index, 10000 + unique.index(material))
    for index in range(len(unique)):
        gs_mat.remap_material_i_ds(target, 10000 + index, index)
    return target, unique


def fit_budget(mesh, slot_count):
    """Simplifies the merged mesh as a whole when it is over the budget. Returns (simplified, problems)."""
    total = mesh.get_triangle_count()
    if total <= TRIANGLE_BUDGET:
        log("  total %d tris: inside the budget, nothing simplified" % total)
        return False, []
    before_box = box_of(mesh)
    before_counts = triangles_per_material(mesh)
    before_uv = {material_id: uv_bounds(mesh, material_id) for material_id in range(slot_count)}
    target = SIMPLIFY_TARGET
    for _attempt in range(3):
        gs_simplify.apply_simplify_to_triangle_count(mesh, int(target), unreal.GeometryScriptSimplifyMeshOptions())
        if mesh.get_triangle_count() <= SIMPLIFY_TARGET + 20:
            break
        target -= mesh.get_triangle_count() - SIMPLIFY_TARGET + 20
    after_box = box_of(mesh)
    after_counts = triangles_per_material(mesh)
    moved = max(abs(getattr(before_box.min, axis) - getattr(after_box.min, axis)) for axis in "xyz")
    moved = max(moved, max(abs(getattr(before_box.max, axis) - getattr(after_box.max, axis)) for axis in "xyz"))
    problems = []
    if moved > BOUNDS_TOLERANCE:
        problems.append("box moved %.2f cm" % moved)
    if set(before_counts) != set(after_counts):
        problems.append("material ids %s -> %s" % (sorted(before_counts), sorted(after_counts)))
    for material_id in sorted(after_counts):
        before = before_uv.get(material_id)
        after = uv_bounds(mesh, material_id)
        if before is None or after is None:
            problems.append("uv box of id %d not read" % material_id)
            continue
        # Only UVs that leave the old box count: a smaller box means triangles of one corner were removed.
        drift = max(before[0] - after[0], before[1] - after[1], after[2] - before[2], after[3] - before[3], 0.0)
        log("    id %d tris %d -> %d, uv box %s -> %s (outside by %.4f)" % (
            material_id, before_counts.get(material_id, 0), after_counts[material_id],
            ["%.3f" % value for value in before], ["%.3f" % value for value in after], drift))
        if drift > UV_TOLERANCE:
            problems.append("uv box of id %d grew by %.3f" % (material_id, drift))
    if mesh.get_triangle_count() > TRIANGLE_BUDGET:
        problems.append("still %d tris, over the budget" % mesh.get_triangle_count())
    log("  simplified %d -> %d tris, box moved %.2f cm, %s" % (
        total, mesh.get_triangle_count(), moved, "OK" if not problems else "PROBLEMS: " + "; ".join(problems)))
    return True, problems


def vertex_positions(mesh):
    result = gs_query.get_all_vertex_positions(mesh, False)
    for item in result if isinstance(result, tuple) else (result,):
        if isinstance(item, unreal.GeometryScriptVectorList):
            return gs_list.convert_vector_list_to_array(item)
    return []


def upper_part_text(part, level):
    """Where a part has geometry above the given height (shows which part carries the turret and the gun)."""
    upper = [point for point in vertex_positions(part["mesh"]) if point.z > level]
    if not upper:
        return "above z %.0f: %s has nothing" % (level, part["component"])
    return "above z %.0f: %s has %d vertices, x %.1f..%.1f y %.1f..%.1f" % (
        level, part["component"], len(upper), min(point.x for point in upper), max(point.x for point in upper),
        min(point.y for point in upper), max(point.y for point in upper))


def height_profile(parts, x0, x1, y_centre, y_half):
    """Highest point per slice along X (PROFILE_BINS slices of x0..x1, middle half of the width); None = empty."""
    tops = [None] * PROFILE_BINS
    for part in parts:
        for point in vertex_positions(part["mesh"]):
            if abs(point.y - y_centre) > y_half * 0.5 or not x0 <= point.x < x1:
                continue
            index = int((point.x - x0) / (x1 - x0) * PROFILE_BINS)
            if tops[index] is None or point.z > tops[index]:
                tops[index] = point.z
    return tops


def profile_error(first, second):
    pairs = [(a, b) for a, b in zip(first, second) if a is not None and b is not None]
    if len(pairs) < PROFILE_BINS // 2:
        return None
    mean_a = sum(a for a, _b in pairs) / len(pairs)
    mean_b = sum(b for _a, b in pairs) / len(pairs)
    return sum(abs((a - mean_a) - (b - mean_b)) for a, b in pairs) / len(pairs)


def wreck_front_text(tank_name, parts):
    """Compares the wreck hull with the live hull of the same run: height profile along X, as is and mirrored."""
    live = LIVE_PROFILES.get(tank_name)
    if live is None:
        return "wreck front: no live hull of this run to compare with"
    x0, x1, y_centre, y_half, live_profile = live
    wreck_profile = height_profile(footprint_parts(parts, True), x0, x1, y_centre, y_half)
    straight = profile_error(live_profile, wreck_profile)
    mirrored = profile_error(live_profile, list(reversed(wreck_profile)))
    if straight is None or mirrored is None:
        return "wreck front: profiles could not be compared"
    if straight < mirrored * 0.8:
        verdict = "+X OK"
    elif mirrored < straight * 0.8:
        verdict = "NOT +X"
    else:
        verdict = "UNDECIDED"
    return "wreck front %s: hull height profile differs from the live hull by %.1f cm as is, %.1f cm mirrored" % (
        verdict, straight, mirrored)


def front_check(parts, merged, is_wreck, foot_low, foot_high):
    """Which X end is the front. Rule 1: the turret actor's parts reach further from the turret pivot on the gun side.
    Rule 2 (also for tanks without a turret actor): above 55 % of the height, the tank reaches further from the hull
    centre on the gun side."""
    all_low, all_high = union([part["box"] for part in parts])
    centre_x = (foot_low[0] + foot_high[0]) * 0.5
    text = "overhang past the footprint: +X %.1f, -X %.1f" % (all_high[0] - foot_high[0], foot_low[0] - all_low[0])
    if is_wreck:
        return "wreck (no gun rule); " + text
    verdicts = []
    turret = [part for part in parts if not part["on_root_actor"]]
    if turret:
        low, high = union([part["box"] for part in turret])
        pivot_x = turret[0]["pivot_x"]
        text += "; turret parts reach +X %.1f / -X %.1f from the turret pivot" % (high[0] - pivot_x, pivot_x - low[0])
        verdicts.append(high[0] - pivot_x > pivot_x - low[0])
    points = vertex_positions(merged)
    if points:
        level = all_low[2] + (all_high[2] - all_low[2]) * 0.55
        upper = [point.x for point in points if point.z > level]
        if upper:
            text += "; above 55 %% of the height the tank reaches +X %.1f / -X %.1f from the hull centre" % (
                max(upper) - centre_x, centre_x - min(upper))
            verdicts.append(max(upper) - centre_x > centre_x - min(upper))
    if not verdicts:
        verdict = "UNKNOWN"
    elif all(verdicts):
        verdict = "+X OK"
    elif not any(verdicts):
        verdict = "NOT +X"
    else:
        verdict = "RULES DISAGREE"
    return "front %s; %s" % (verdict, text)


# ---------------------------------------------------------------------------------------------------------------------
# Asset
# ---------------------------------------------------------------------------------------------------------------------

def write_static_mesh(name, dynamic_mesh, materials, recompute_tangents):
    path = tank_config.DEST + "/" + name
    slot_names = []
    for material in materials:
        slot = material.get_name() if material is not None else "None"
        while slot in slot_names:
            slot += "_2"
        slot_names.append(slot)
    existed = assets.does_asset_exist(path)
    if existed:
        static_mesh = unreal.load_asset(path)
        options = unreal.GeometryScriptCopyMeshToAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = recompute_tangents
        options.replace_materials = True
        options.new_materials = materials
        options.new_material_slot_names = slot_names
        options.emit_transaction = False
        options.defer_mesh_post_edit_change = False
        gs_assets.copy_mesh_to_static_mesh(dynamic_mesh, static_mesh, options, unreal.GeometryScriptMeshWriteLOD(), False)
    else:
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = recompute_tangents
        options.enable_nanite = False
        options.enable_collision = False
        options.collision_mode = unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX
        static_mesh, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            dynamic_mesh, path, options)
        if outcome != SUCCESS:
            log("  CREATE FAILED %s: %s" % (path, outcome))
            return None
        static_mesh = unreal.load_asset(path)
    static_mesh.set_editor_property("static_materials", [
        unreal.StaticMaterial(material_interface=material, material_slot_name=slot_names[index])
        for index, material in enumerate(materials)])
    nanite = static_mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    static_mesh.set_editor_property("nanite_settings", nanite)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(static_mesh)
    body = static_mesh.get_editor_property("body_setup")
    if body is not None:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    build = subsystem.get_lod_build_settings(static_mesh, 0)
    build.generate_lightmap_u_vs = False
    build.recompute_normals = False
    build.recompute_tangents = recompute_tangents
    subsystem.set_lod_build_settings(static_mesh, 0, build)
    static_mesh.set_editor_property("light_map_resolution", 4)
    saved = assets.save_loaded_asset(static_mesh, False)
    file_name = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len("/Game/"):] + ".uasset"
    age = time.time() - os.path.getmtime(file_name) if os.path.exists(file_name) else -1.0
    log("  %s %s: save=%s, file written %.0f s ago%s" % ("updated" if existed else "created", path, saved, age,
                                                       "" if saved and 0.0 <= age < 120.0 else "  SAVE FAILED"))
    return static_mesh


def build_piece(name, blueprint_path, is_wreck, do_build):
    log("-- %s from %s%s" % (name, blueprint_path, "" if do_build else "  (dry: parts only)"))
    parts = gather_wreck(blueprint_path) if is_wreck else gather_parts(blueprint_path)
    if not parts:
        log("  no visible mesh parts")
        return None
    sources = sorted(set(part["source"] for part in parts))
    part_texts = ["%s %d tris" % (part["source"].split("/")[-1], part["triangles"]) for part in parts]
    foot = footprint_parts(parts, is_wreck)
    foot_low, foot_high = union([part["box"] for part in foot])
    all_low, all_high = union([part["box"] for part in parts])
    log("  parts %d, total %d tris; footprint (%s) x %.1f..%.1f y %.1f..%.1f; all parts z %.1f..%.1f" % (
        len(parts), sum(part["triangles"] for part in parts), ", ".join(part["component"] for part in foot),
        foot_low[0], foot_high[0], foot_low[1], foot_high[1], all_low[2], all_high[2]))
    if not is_wreck:
        for part in parts:
            log("  " + upper_part_text(part, all_low[2] + (all_high[2] - all_low[2]) * 0.6))
    tank_name = name.split("_", 2)[2]
    if is_wreck:
        log("  " + wreck_front_text(tank_name, parts))
    else:
        LIVE_PROFILES[tank_name] = (foot_low[0], foot_high[0], (foot_low[1] + foot_high[1]) * 0.5,
                                    (foot_high[1] - foot_low[1]) * 0.5,
                                    height_profile(foot, foot_low[0], foot_high[0], (foot_low[1] + foot_high[1]) * 0.5,
                                                   (foot_high[1] - foot_low[1]) * 0.5))
    merged, materials = merge(parts)
    log("  " + front_check(parts, merged, is_wreck, foot_low, foot_high))
    if not do_build:
        return None
    before = merged.get_triangle_count()
    simplified, problems = fit_budget(merged, len(materials))
    box = box_of(merged)
    offset = unreal.Vector(-(foot_low[0] + foot_high[0]) * 0.5, -(foot_low[1] + foot_high[1]) * 0.5, -box.min.z)
    gs_xform.translate_mesh(merged, offset)
    box = box_of(merged)
    log("  pivot offset applied %s; merged %d tris, box min %s max %s" % (
        vec(offset), merged.get_triangle_count(), vec(box.min), vec(box.max)))
    counts = triangles_per_material(merged)
    for index, material in enumerate(materials):
        uv = uv_bounds(merged, index)
        log("  slot %d %s: %d tris, uv box %s" % (index, short(material), counts.get(index, 0),
                                                ["%.3f" % value for value in uv] if uv else None))
    static_mesh = write_static_mesh(name, merged, materials, simplified)
    if static_mesh is None:
        return None
    bounds = static_mesh.get_bounds()
    log("  asset tris=%d lods=%d slots=%d extent=%s origin=%s%s" % (
        static_mesh.get_num_triangles(0), static_mesh.get_num_lods(), len(static_mesh.static_materials),
        vec(bounds.box_extent), vec(bounds.origin), "" if not problems else "  PROBLEMS: " + "; ".join(problems)))
    return {"blueprint": blueprint_path, "sources": sources, "simplified_from": before if simplified else 0,
            "problems": problems, "parts": part_texts}


def run():
    build_names = [entry.strip() for entry in os.environ.get("DIVISIONTANKS", "").split(",") if entry.strip()]
    dry_names = [entry.strip() for entry in os.environ.get("DIVISIONTANKS_DRY", "").split(",") if entry.strip()]
    log("=" * 20 + " build run %s build=%s dry=%s" % (time.strftime("%Y-%m-%d %H:%M:%S"), build_names or "ALL", dry_names))
    if not assets.does_directory_exist(tank_config.DEST):
        assets.make_directory(tank_config.DEST)
    sources = {}
    if os.path.exists(SOURCES_PATH):
        with open(SOURCES_PATH, "r", encoding="utf-8") as handle:
            sources = json.load(handle)
    for tank in tank_config.TANKS:
        do_build = not build_names or tank["name"] in build_names
        if not do_build and "ALL" not in dry_names and tank["name"] not in dry_names:
            continue
        try:
            result = build_piece("SM_DivisionTank_" + tank["name"], tank["vehicle"], False, do_build)
            if result is not None:
                sources["SM_DivisionTank_" + tank["name"]] = result
        except Exception:
            log("ERROR %s\n%s" % (tank["name"], traceback.format_exc()))
        wreck_name = "SM_DivisionTankWreck_" + tank["name"]
        made = False
        for wreck in tank["wrecks"]:
            try:
                if not assets.does_asset_exist(wreck):
                    log("-- %s: no wreck Blueprint %s" % (wreck_name, wreck))
                    continue
                result = build_piece(wreck_name, wreck, True, do_build)
                if result is not None:
                    sources[wreck_name] = result
                    made = True
                if made or not do_build:
                    break
            except Exception:
                log("ERROR %s\n%s" % (wreck_name, traceback.format_exc()))
        if do_build and not made:
            log("-- %s NOT MADE" % wreck_name)
        with open(SOURCES_PATH, "w", encoding="utf-8") as handle:
            json.dump(sources, handle, indent=1)


try:
    run()
except Exception:
    log(traceback.format_exc())
log("DONE")
HANDLE.close()
