"""Headless, read-only: exports the Ju 87 VTO base colour and camo textures to PNG to judge them (normal sRGB textures;
the PNG exporter is picked only when it supports the texture). Output: Saved/WorldBuildSitePieces/airfield_C/tex/"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_C", "tex")
os.makedirs(OUT, exist_ok=True)
for name in ("BaseColor", "Camo"):
    texture = unreal.load_asset("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Materials/Plane_stuka_01_" + name)
    if texture is None:
        unreal.log("AIRFIELDC no texture " + name)
        continue
    exporter = unreal.TextureExporterPNG()
    if not exporter.supports_object(texture) if hasattr(exporter, "supports_object") else False:
        unreal.log("AIRFIELDC PNG exporter does not support " + name)
        continue
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = os.path.join(OUT, name + ".png")
    task.exporter = exporter
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    unreal.log("AIRFIELDC export %s ok=%s" % (name, unreal.Exporter.run_asset_export_task(task)))
