"""Exports the Breakthrough concrete material as text and lists the UV ranges of the concrete slots of the pieces.

Log: Saved/WorldMapPieces/concrete.txt, material text: Saved/WorldMapPieces/M_BF_SootedFortress_Procedural.t3d
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402

BUNKERS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/"
MATERIAL = BUNKERS + "FlakBunker/Material/M_BF_SootedFortress_Procedural"
UV_TILE = BUNKERS + "BreakthroughShared/MI_BF_SootedFortress_UVTile"

task = unreal.AssetExportTask()
task.object = unreal.load_asset(MATERIAL)
task.filename = os.path.join(lib.OUT_DIR, "M_BF_SootedFortress_Procedural.t3d")
task.automated = True
task.replace_identical = True
task.prompt = False
lib.log("export ok: %s" % unreal.Exporter.run_asset_export_task(task))

instance = unreal.load_asset(UV_TILE)
for name in unreal.MaterialEditingLibrary.get_static_switch_parameter_names(instance):
    lib.log("switch %s = %s" % (
        name, unreal.MaterialEditingLibrary.get_material_instance_static_switch_parameter_value(instance, name)))

MESHES = {
    lib.STORY_ROOT + "/OderWarthe/SM_WorldPiece_OderWarthe": ("Concrete", "TeethConcrete"),
    lib.STORY_ROOT + "/OreMountains/SM_WorldPiece_OreMountains": ("Concrete",),
    "/Game/RTS_Survival/Blueprints/Environment/Berlin/FlakTower/SM_BF_FlakTower_Damaged": ("Concrete",),
}
for path, wanted in MESHES.items():
    mesh = unreal.load_asset(path)
    description = mesh.get_static_mesh_description(0)
    lib.log("=== %s" % mesh.get_name())
    slot_names = [str(slot.material_slot_name) for slot in mesh.static_materials]
    for group in range(description.get_polygon_group_count()):
        group_id = unreal.PolygonGroupID(group)
        slot_name = str(description.get_polygon_group_material_slot_name(group_id))
        if not any(slot_name.endswith(suffix) for suffix in wanted):
            continue
        positions, uvs = [], []
        for polygon in description.get_polygon_group_polygons(group_id):
            for instance_id in description.get_polygon_vertex_instances(polygon):
                uvs.append(description.get_vertex_instance_uv(instance_id, 0))
                positions.append(description.get_vertex_position(
                    description.get_vertex_instance_vertex(instance_id)))
        if not uvs:
            continue
        lib.log("  %s (%s): %d corners, u %.2f..%.2f v %.2f..%.2f, x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f" % (
            slot_name, slot_names, len(uvs), min(uv.x for uv in uvs), max(uv.x for uv in uvs),
            min(uv.y for uv in uvs), max(uv.y for uv in uvs),
            min(p.x for p in positions), max(p.x for p in positions), min(p.y for p in positions),
            max(p.y for p in positions), min(p.z for p in positions), max(p.z for p in positions)))
lib.write_log("concrete.txt")
