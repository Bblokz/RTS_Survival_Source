"""Exports the static part of the German airbase building to FBX, only to look at it in Blender. Read-only.

Skeletal meshes are not exported: the FBX skeletal exporter needs CPU-skinned render data and asserts under -nullrhi
(USkinnedMeshComponent::GetCPUSkinnedVertices). The roof and the Ju 87 are read with GeometryScript instead
(build_airfield_a.py).

Run headless: sh run_headless.sh export_source   Output: Saved/WorldBuildSitePieces/airfield_candidate_A/src/base.fbx
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_candidate_A", "src")
SOURCE = ("/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/"
          "BuildingMeshes/SM_Ger_AircraftBuilding_BaseMesh")

os.makedirs(OUT, exist_ok=True)
asset = unreal.load_asset(SOURCE)
if asset is None:
    unreal.log("AIRFIELD_A MISSING " + SOURCE)
else:
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(OUT, "base.fbx")
    task.exporter = unreal.StaticMeshExporterFBX()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    task.options = options
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log("AIRFIELD_A export -> %s ok=%s exists=%s" % (task.filename, ok, os.path.exists(task.filename)))
