"""Headless read-only check of the saved division tank pieces. Writes the delivery log.

Run:  sh run_py.sh check_tank_pieces.py [T34_76,...]
Logs: Saved/WorldDivisionPieces/tank_pieces.txt   one line per mesh (all tanks of tank_config.py)
      Saved/WorldDivisionPieces/tank_check.txt    details: sections, triangles and UV box per slot, material flags
Loads each saved asset from disk in a fresh process, so it shows what is really in the .uasset files.
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
TRIANGLE_BUDGET = 4000
PIECE_LINES = []
DETAIL = open(os.path.join(OUT, "tank_check.txt"), "w", encoding="utf-8")
gs_assets = unreal.GeometryScript_AssetUtils
gs_mat = unreal.GeometryScript_Materials
gs_select = unreal.GeometryScript_MeshSelection
gs_uvs = unreal.GeometryScript_UVs
gs_list = unreal.GeometryScript_List
gs_query = unreal.GeometryScript_MeshQueries


def detail(text):
    DETAIL.write(str(text) + "\n")
    DETAIL.flush()


def vec(v):
    return "(%.1f %.1f %.1f)" % (v.x, v.y, v.z)


def short(obj):
    return obj.get_path_name().split(".")[0] if obj is not None else "None"


def triangles_per_material(mesh):
    result = gs_mat.get_all_triangle_material_i_ds(mesh)
    for item in result if isinstance(result, tuple) else (result,):
        if isinstance(item, unreal.GeometryScriptIndexList):
            counts = {}
            for material_id in gs_list.convert_index_list_to_array(item):
                counts[material_id] = counts.get(material_id, 0) + 1
            return counts
    return {}


def uv_bounds(mesh, material_id):
    try:
        selection = gs_select.select_mesh_elements_by_material_id(
            mesh, material_id, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
        if isinstance(selection, tuple):
            selection = [item for item in selection if isinstance(item, unreal.GeometryScriptMeshSelection)][-1]
        result = gs_uvs.get_mesh_uv_size_info(mesh, 0, selection)
        for item in result if isinstance(result, tuple) else (result,):
            if isinstance(item, unreal.Box2D):
                return "u %.3f..%.3f v %.3f..%.3f" % (item.min.x, item.max.x, item.min.y, item.max.y)
    except Exception as error:
        return "unavailable (%s)" % error
    return "unavailable"


def material_flags(material):
    if material is None:
        return "None"
    base = material.get_base_material()
    text = "base %s" % (base.get_name() if base else None)
    if base is not None:
        for flag in ("used_with_instanced_static_meshes", "used_with_nanite", "two_sided"):
            try:
                text += " %s=%s" % (flag, base.get_editor_property(flag))
            except Exception:
                pass
        try:
            text += " blend=%s" % str(base.get_editor_property("blend_mode")).split(".")[-1].split(":")[0]
        except Exception:
            pass
    return text


def check_mesh(name, sources):
    path = tank_config.DEST + "/" + name
    file_name = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len("/Game/"):] + ".uasset"
    if not unreal.EditorAssetLibrary.does_asset_exist(path) or not os.path.exists(file_name):
        PIECE_LINES.append("%s | NOT MADE" % name)
        detail("== %s NOT MADE" % name)
        return
    mesh = unreal.load_asset(path)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    triangles = mesh.get_num_triangles(0)
    lods = mesh.get_num_lods()
    bounds = mesh.get_bounds()
    box = mesh.get_bounding_box()
    nanite = mesh.get_editor_property("nanite_settings").enabled
    collisions = subsystem.get_simple_collision_count(mesh)
    slots = ["%d %s=%s" % (index, slot.material_slot_name, short(slot.material_interface))
             for index, slot in enumerate(mesh.static_materials)]
    info = sources.get(name, {})
    problems = []
    if triangles > TRIANGLE_BUDGET:
        problems.append("over %d triangles" % TRIANGLE_BUDGET)
    if abs(box.min.z) > 0.1:
        problems.append("base not at Z 0 (%.2f)" % box.min.z)
    if nanite:
        problems.append("nanite on")
    if collisions != 0:
        problems.append("%d collision shapes" % collisions)
    if lods != 1:
        problems.append("%d LODs" % lods)
    if any(slot.material_interface is None for slot in mesh.static_materials):
        problems.append("empty material slot")
    problems += info.get("problems", [])
    PIECE_LINES.append(
        "%s | tris %d | extent %s | origin %s | box min %s max %s | lods %d | nanite %s | collision shapes %d | "
        "slots [%s] | sources [%s] | from %s | %s | file written %s | %s" % (
            name, triangles, vec(bounds.box_extent), vec(bounds.origin), vec(box.min), vec(box.max), lods, nanite,
            collisions, "; ".join(slots), "; ".join(info.get("parts", ["?"])), info.get("blueprint", "?"),
            "simplified from %d tris" % info["simplified_from"] if info.get("simplified_from") else "not simplified",
            time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(os.path.getmtime(file_name))),
            "OK" if not problems else "PROBLEMS: " + "; ".join(problems)))

    detail("== %s  file %s (%d bytes, written %s)" % (name, file_name, os.path.getsize(file_name),
                                                      time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(os.path.getmtime(file_name)))))
    def step(label, action):
        try:
            action()
        except Exception as error:
            detail("   %s: check step failed: %s" % (label, error))

    step("counts", lambda: detail("   tris %d, vertices %d, lods %d, nanite %s, collision shapes %d" % (
        triangles, subsystem.get_number_verts(mesh, 0), lods, nanite, collisions)))
    step("complexity", lambda: detail("   collision complexity %s" % subsystem.get_collision_complexity(mesh)))
    detail("   box min %s max %s; size %.0f x %.0f x %.0f cm" % (
        vec(box.min), vec(box.max), box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z))

    def sections():
        for section in range(mesh.get_num_sections(0)):
            detail("   section %d -> slot %d" % (section, subsystem.get_lod_material_slot(mesh, 0, section)))
    step("sections", sections)

    def per_slot():
        dynamic = unreal.DynamicMesh()
        options = unreal.GeometryScriptCopyMeshFromAssetOptions()
        gs_assets.copy_mesh_from_static_mesh_v2(mesh, dynamic, options, unreal.GeometryScriptMeshReadLOD(), False)
        counts = triangles_per_material(dynamic)
        for index, slot in enumerate(mesh.static_materials):
            detail("   slot %d %s -> %s: %d tris, uv0 box %s, %s" % (
                index, slot.material_slot_name, short(slot.material_interface), counts.get(index, 0),
                uv_bounds(dynamic, index), material_flags(slot.material_interface)))
        stray = sorted(material_id for material_id in counts if material_id >= len(mesh.static_materials))
        empty = [index for index in range(len(mesh.static_materials)) if counts.get(index, 0) == 0]
        if stray or empty:
            detail("   PROBLEM: material ids without slot %s, slots without triangles %s" % (stray, empty))
            PIECE_LINES[-1] += "; material ids without slot %s, slots without triangles %s" % (stray, empty)
        detail("   uv sets %s, read back %d tris" % (gs_query.get_num_uv_sets(dynamic), dynamic.get_triangle_count()))
    step("slots", per_slot)
    detail("   source parts: %s" % "; ".join(info.get("parts", ["?"])))
    for source in info.get("sources", []):
        detail("   source asset %s" % source)


def run():
    sources = {}
    path = os.path.join(OUT, "tank_sources.json")
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as handle:
            sources = json.load(handle)
    for tank in tank_config.selected(os.environ.get("DIVISIONTANKS", "")):
        for prefix in ("SM_DivisionTank_", "SM_DivisionTankWreck_"):
            try:
                check_mesh(prefix + tank["name"], sources)
            except Exception:
                detail("ERROR %s\n%s" % (prefix + tank["name"], traceback.format_exc()))
                if PIECE_LINES and PIECE_LINES[-1].startswith(prefix + tank["name"] + " |"):
                    PIECE_LINES[-1] += " | DETAIL CHECK ERROR (see tank_check.txt)"
                else:
                    PIECE_LINES.append("%s | CHECK ERROR (see tank_check.txt)" % (prefix + tank["name"]))


try:
    run()
except Exception:
    detail(traceback.format_exc())
with open(os.path.join(OUT, "tank_pieces.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(PIECE_LINES) + "\n")
DETAIL.close()
unreal.log("DIVTANK check written, %d lines" % len(PIECE_LINES))
