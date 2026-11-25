"""Headless probe: describes the blood textures/materials/Niagara systems the crush blood builds on, exports T_blood.

Run: UnrealEditor.exe <uproject> -nullrhi -ExecutePythonScript=<this> -ABSLOG=<log>
Writes Saved/VehicleCrush/inspect.json and Saved/VehicleCrush/T_blood.tga.
"""
import json
import os
import unreal

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "VehicleCrush")
os.makedirs(OUT_DIR, exist_ok=True)
report = {}

VFX = "/Game/RTS_Survival/SpecialEffects/Realistic_Starter_VFX_Pack/"
for path in [VFX + "Textures/T_blood", VFX + "Materials/M_blood_b", VFX + "Materials/M_blood_c",
             VFX + "Materials/M_VAT_Splat_Blood", VFX + "Materials/M_VAT_Splat_Blood_Inst",
             VFX + "Particles/Blood/P_Blood_Splat_2d_Converted", VFX + "Particles/Blood/P_Blood_Splat_3d_Converted",
             VFX + "Particles/Hit/P_Blood_Converted1"]:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    entry = {"class": asset.get_class().get_name() if asset else None}
    if isinstance(asset, unreal.Texture2D):
        entry.update({"size": [asset.blueprint_get_size_x(), asset.blueprint_get_size_y()],
                      "srgb": asset.get_editor_property("srgb"),
                      "compression": str(asset.get_editor_property("compression_settings"))})
        task = unreal.AssetExportTask()
        task.object = asset
        task.filename = os.path.join(OUT_DIR, "T_blood.tga")
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        entry["exported"] = unreal.Exporter.run_asset_export_task(task)
    if isinstance(asset, unreal.MaterialInterface):
        base = asset.get_base_material()
        entry["domain"] = str(base.get_editor_property("material_domain"))
        entry["blend"] = str(base.get_editor_property("blend_mode"))
        entry["textures"] = [t.get_path_name() for t in
                             unreal.MaterialEditingLibrary.get_used_textures(base)]
    if isinstance(asset, unreal.NiagaraSystem):
        entry["emitters"] = [str(h.get_editor_property("name")) if hasattr(h, "get_editor_property") else str(h)
                             for h in []]
    report[path] = entry

with open(os.path.join(OUT_DIR, "inspect.json"), "w") as fh:
    json.dump(report, fh, indent=1)
unreal.log("VEHICLE_CRUSH_INSPECT_DONE")
unreal.SystemLibrary.quit_editor()
