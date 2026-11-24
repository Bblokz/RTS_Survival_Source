"""Exports reference meshes (existing silos etc.) to FBX for the Blender layout/preview scene. Read-only on assets."""
import os

import unreal

OUT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreSites/Model_OrePlant_HeapStation/refs'
MESHES = ['/Game/QuarryEnv/Meshes/SM_Silo_1', '/Game/QuarryEnv/Meshes/SM_Silo_2', '/Game/QuarryEnv/Meshes/SM_Silo_3',
          '/Game/QuarryEnv/Meshes/SM_Silo_4', '/Game/MillitaryBase/Meshes/SM_Silos',
          '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/Meshes/SM_Scorch_Silo2',
          '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/AmmoStorageMeshes/SM_Rus_AmmoStorage']
os.makedirs(OUT, exist_ok=True)
for p in MESHES:
    m = unreal.load_asset(p)
    task = unreal.AssetExportTask()
    task.object = m
    task.filename = '%s/%s.fbx' % (OUT, p.split('/')[-1])
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.StaticMeshExporterFBX()
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log_warning('OREPLANT_EXPORT %s %s' % (p, ok))
