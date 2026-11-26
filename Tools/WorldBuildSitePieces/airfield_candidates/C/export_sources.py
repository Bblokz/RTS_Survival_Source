"""Exports the German airbase building (base mesh, sliding roof + its open animation) and the Ju 87 VTO to FBX for the
Blender step. Read-only for the assets.

Run headless: sh run_py.sh export_sources.py    Output: Saved/WorldBuildSitePieces/airfield_C/src/*.fbx
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "airfield_C", "src")
AB = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/BuildingMeshes/"
JOBS = [
    (AB + "SM_Ger_AircraftBuilding_BaseMesh", "base.fbx", unreal.StaticMeshExporterFBX),
    (AB + "SK_Ger_AircraftBuilding_Roof", "roof.fbx", unreal.SkeletalMeshExporterFBX),
    (AB + "Anims/Anim_GerAirbase_OpenStationary", "roof_open_anim.fbx", unreal.AnimSequenceExporterFBX),
    (AB + "Anims/Anim_GerAirbase_CloseToOpen", "roof_close_to_open_anim.fbx", unreal.AnimSequenceExporterFBX),
    ("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO", "stuka.fbx",
     unreal.SkeletalMeshExporterFBX),
]

os.makedirs(OUT, exist_ok=True)
for path, name, exporter in JOBS:
    asset = unreal.load_asset(path)
    if asset is None:
        unreal.log("AIRFIELDC MISSING " + path)
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(OUT, name)
    task.exporter = exporter()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    # Material baking renders the mesh (CPU skinning): asserts under -nullrhi for skeletal meshes.
    try:
        options.set_editor_property("bake_material_inputs", unreal.FbxMaterialBakeMode.DISABLED)
    except Exception as error:
        unreal.log("AIRFIELDC bake option: %s" % error)
    if os.path.exists(task.filename):
        unreal.log("AIRFIELDC already there " + task.filename)
        continue
    if exporter is unreal.AnimSequenceExporterFBX:
        options.export_preview_mesh = False
    task.options = options
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log("AIRFIELDC export %s -> %s ok=%s exists=%s" % (path, task.filename, ok, os.path.exists(task.filename)))
