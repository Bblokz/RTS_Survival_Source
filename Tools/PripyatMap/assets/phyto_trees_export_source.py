"""Headless: export SM_RTS_DeadTree_1 from UE for the Blender Phytophage tree build (generate_phyto_trees.py reads both).

    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
        -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Source/SM_RTS_DeadTree_1_FromUE.FBX       LOD0 + the six UCX collision hulls
Source/SM_RTS_DeadTree_1_FromUE_LODs.FBX  LOD0-2 (the bark of the variants' LOD1/2 is the source's own LOD)
"""
import os

import unreal

OUT_DIR = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Phytophage/Trees/Source"
os.makedirs(OUT_DIR, exist_ok=True)
mesh = unreal.EditorAssetLibrary.load_asset("/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/SM_RTS_DeadTree_1")
for name, collision, lods in (("SM_RTS_DeadTree_1_FromUE.FBX", True, False),
                              ("SM_RTS_DeadTree_1_FromUE_LODs.FBX", False, True)):
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = OUT_DIR + "/" + name
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    task.exporter = unreal.StaticMeshExporterFBX()
    opts = unreal.FbxExportOption()
    opts.collision = collision
    opts.level_of_detail = lods
    opts.vertex_color = True
    task.options = opts
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log_warning("PHYTO_EXPORT %s %s exists=%s" % (ok, task.filename, os.path.exists(task.filename)))
