"""Read-only: exports every audited BXP Blueprint to T3D (Saved/BxpQA/t3d/) for graph inspection."""
import os
import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/BxpQA/t3d/"
os.makedirs(OUT, exist_ok=True)
for line in open("D:/UE5Projects/RTS_Survival/Source/Tools/BxpQA/bxp_list.txt"):
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    path = line.split()[1]
    asset = unreal.load_asset(path)
    if asset is None:
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = OUT + path.split("/")[-1] + ".t3d"
    task.exporter = unreal.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    unreal.log("BXPQA t3d %s %s" % (path, unreal.Exporter.run_asset_export_task(task)))
