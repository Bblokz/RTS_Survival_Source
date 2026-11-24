"""Exports the BXP meshes that get procedural holes to FBX (+ slots.json) for the Blender debris cutter."""
import json
import unreal

OUT = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/BxpQA_Debris/Exports/"
MESHES = [l.strip() for l in open("D:/UE5Projects/RTS_Survival/Source/Tools/BxpQA/debris_meshes.txt") if l.strip()]
slots = {}
for path in MESHES:
    mesh = unreal.load_asset(path)
    if mesh is None:
        unreal.log_error("BXPQA missing " + path)
        continue
    name = mesh.get_name()
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = OUT + name + ".fbx"
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    opts = unreal.FbxExportOption()
    opts.set_editor_property("collision", False)
    opts.set_editor_property("level_of_detail", False)
    opts.set_editor_property("export_source_mesh", True)
    task.options = opts
    ok = unreal.Exporter.run_asset_export_task(task)
    slots[name] = {"ue_path": path, "slots": [[str(s.get_editor_property("material_slot_name")),
                                               s.get_editor_property("material_interface").get_path_name()
                                               if s.get_editor_property("material_interface") else None]
                                              for s in mesh.get_editor_property("static_materials")]}
    unreal.log("BXPQA export %s %s" % (name, ok))
json.dump(slots, open(OUT + "slots.json", "w"), indent=1)
unreal.log("BXPQA export done %d" % len(slots))
