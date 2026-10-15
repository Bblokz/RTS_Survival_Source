"""Exports the ZiS-3 team weapon (skeleton, mesh and animations) the 45 mm 53-K is rigged against.

The 53-K shares the ZiS-3 skeleton (same bone names, hierarchy and mesh-space frame) so the ZiS-3 anim blueprint,
team weapon and squad can be copied with only their animations and mesh swapped.
Run as a commandlet (skeletal FBX export needs rendering):
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -AllowCommandletRendering
"""
import unreal

OUT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/45mmAT_53K/Source'
ZIS = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/Zis_76MM/'
MESHES = [ZIS + 'Mesh/SK_TW_Zis3']
ANIMS = [ZIS + 'DeployPacked/Anim_Zis3_Deployed', ZIS + 'DeployPacked/Anim_Zis3_Packed',
         ZIS + 'DeployPacked/Anim_Zis3_DeployedToPacked', ZIS + 'WeaponAnims/Anim_Zis3_Base',
         ZIS + 'WeaponAnims/Anim_Zis3_Left25', ZIS + 'WeaponAnims/Anim_Zis3_Up30', ZIS + 'WeaponAnims/Anim_Zis3_Fire',
         ZIS + 'WheelsAnims/Anim_Zis3_WheelsFWD']


def export(asset, exporter):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = '%s/%s.fbx' % (OUT_DIR, asset.get_name())
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    task.exporter = exporter
    unreal.log_warning('AT53K_EXPORT %s %s' % (asset.get_name(), unreal.Exporter.run_asset_export_task(task)))


for path in MESHES:
    export(unreal.load_asset(path), unreal.SkeletalMeshExporterFBX())
for path in ANIMS:
    export(unreal.load_asset(path), unreal.AnimSequenceExporterFBX())
