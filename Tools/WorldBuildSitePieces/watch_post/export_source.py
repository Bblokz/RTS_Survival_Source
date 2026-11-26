"""Exports the Soviet metal guard tower (SM_Rus_GuardMgTower) to FBX for the Blender step. Read-only for the asset.

Run headless (run_headless.sh export_source). Output: Saved/WorldBuildSitePieces/watch_post/SM_Rus_GuardMgTower.fbx
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "watch_post")
SOURCE = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/GuardMGTower/SM_Rus_GuardMgTower"

os.makedirs(OUT, exist_ok=True)
mesh = unreal.load_asset(SOURCE)
task = unreal.AssetExportTask()
task.object = mesh
task.filename = os.path.join(OUT, "SM_Rus_GuardMgTower.fbx")
task.exporter = unreal.StaticMeshExporterFBX()
task.automated = True
task.prompt = False
task.replace_identical = True
options = unreal.FbxExportOption()
options.collision = False
options.level_of_detail = False
task.options = options
ok = unreal.Exporter.run_asset_export_task(task)
unreal.log("WATCHPOST export %s -> %s exists=%s" % (ok, task.filename, os.path.exists(task.filename)))
