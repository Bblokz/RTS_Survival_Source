"""German BXP QA: imports the cut hole-debris pieces (BxpQA_Debris/Debris/<Mesh>/<Mesh>_Debris_0..5.fbx) into
GerBxps/SharedHoles/Debris/<Mesh>/ (headless). Each piece's one slot gets the source mesh's material of the same slot
name (fallback: same material asset name), Nanite off, no collision. bxp_holes_convert.py picks them up by path."""
import json
import os
import unreal

SRC = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/BxpQA_Debris/"
DEST = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/SharedHoles/Debris/"
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
slots_info = json.load(open(SRC + "Exports/slots.json", encoding="utf-8"))


def log(msg):
    unreal.log("BXPDEBRIS " + msg)


def import_piece(filename, destination, name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    data = ui.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    TOOLS.import_asset_tasks([task])
    return unreal.load_asset(destination + "/" + name)


def material_for(slot_name, source_mesh):
    by_slot, by_material = {}, {}
    for sm in source_mesh.get_editor_property("static_materials"):
        material = sm.get_editor_property("material_interface")
        by_slot[str(sm.get_editor_property("material_slot_name"))] = material
        if material is not None:
            by_material[material.get_name()] = material
    return by_slot.get(slot_name) or by_material.get(slot_name) or by_material.get(slot_name.split(".")[0])


def main():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    done, unmapped = 0, []
    try:
        for mesh_name, info in sorted(slots_info.items()):
            folder = SRC + "Debris/" + mesh_name + "/"
            if not os.path.isdir(folder):
                continue
            source = unreal.load_asset(info["ue_path"])
            for index in range(6):
                fbx = folder + "%s_Debris_%d.fbx" % (mesh_name, index)
                if not os.path.exists(fbx):
                    unmapped.append(fbx)
                    continue
                piece = import_piece(fbx, DEST + mesh_name, "%s_Debris_%d" % (mesh_name, index))
                for slot_index, sm in enumerate(piece.get_editor_property("static_materials")):
                    slot = str(sm.get_editor_property("material_slot_name"))
                    material = material_for(slot, source)
                    if material is None:
                        unmapped.append("%s slot %s" % (piece.get_name(), slot))
                        continue
                    piece.set_material(slot_index, material)
                nanite = piece.get_editor_property("nanite_settings")
                nanite.set_editor_property("enabled", False)
                piece.set_editor_property("nanite_settings", nanite)
                EAL.save_loaded_asset(piece, False)
                done += 1
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")
    log("imported %d unmapped %d %s" % (done, len(unmapped), unmapped[:20]))


main()
