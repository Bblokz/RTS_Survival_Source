"""Exports what the German gun pits are authored against (soldier, crew animations, guns) to the Blender folder.

Run as a commandlet: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -NullRHI
Writes FBX files to D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Source and a json with the
montage and sequence lengths the crew drill is synced to.
"""
import json
import unreal

OUT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Source'
SQUAD_ANIMS = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/TeamWeaponAnims/'
TW = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/'
LEFH = '/Game/RTS_Survival/Blueprints/Turrets/EmbeddedTurrets/LeFH_105mm/'

MESHES = [
    '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/GermanSoldier/Meshes/SK_WWII_GermanSoldier_varA',
    TW + 'LFH18/Mesh/SK_Ger_SFH18Anim',
    TW + 'Pak40_75MM/Mesh/Sk_FullAnimPak40TW',
    LEFH + 'Mesh/SK_FieldHaubitze',
]
ANIMS = [
    SQUAD_ANIMS + 'Pak/AN_Infantry_LoadPAK',
    SQUAD_ANIMS + 'Pak/AN_Infantry_SpotPAK_Crouched',
    SQUAD_ANIMS + 'Howitzer/AN_Infantry_LoadHowitzer',
    SQUAD_ANIMS + 'Howitzer/AN_Infantry_Crew_AmmoSupply',
    SQUAD_ANIMS + 'Howitzer/AN_Infantry_SpotHowitzer_Standing',
    SQUAD_ANIMS + 'Commander/AN_Infantry_Commando',
    SQUAD_ANIMS + 'Commander/AN_Infantry_CommandoCrouched',
    SQUAD_ANIMS + 'LargeMortar/AN_Infantry_MortarCrew_Crouched',
    SQUAD_ANIMS + 'Idle/Anim_TW_StandingIdle',
    TW + 'Pak40_75MM/Animations/Deploy/Anim_TWPak40_Deployed',
    TW + 'Pak40_75MM/Animations/Weapon/Anim_TWPak40_Fire',
    TW + 'LFH18/PackAnims/Anim_SFH18_Deployed',
    TW + 'LFH18/WeaponAnim/Anim_SFH18_Fire',
    LEFH + 'Anims/Anim_LeFH_18_Base',
    LEFH + 'Anims/Anim_LeFH_18_Fire',
]
MONTAGES = [
    SQUAD_ANIMS + 'Pak/Montage_TW_LoadingPak',
    SQUAD_ANIMS + 'Howitzer/Montage_TW_LoadHowitzer',
    SQUAD_ANIMS + 'Howitzer/Montage_TW_HowitzerExtraLoading',
    SQUAD_ANIMS + 'LargeMortar/Montage_TW_CrouchedLargeMortar',
    TW + 'Pak40_75MM/Animations/Weapon/Montage_Pak40_Fire',
    TW + 'LFH18/WeaponAnim/Montage_SFH18_Fire',
    LEFH + 'Anims/Montage_LeFH_18_Fire',
]


def export(asset, exporter):
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = '%s/%s.fbx' % (OUT_DIR, asset.get_name())
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    task.exporter = exporter
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log_warning('GERPIT_EXPORT %s %s' % (asset.get_name(), ok))


info = {}
for path in MESHES:
    export(unreal.load_asset(path), unreal.SkeletalMeshExporterFBX())
for path in ANIMS:
    anim = unreal.load_asset(path)
    export(anim, unreal.AnimSequenceExporterFBX())
    info[anim.get_name()] = {'length': anim.get_play_length(), 'frames': anim.get_editor_property('number_of_sampled_frames')
                             if hasattr(anim, 'number_of_sampled_frames') else None}
for path in MONTAGES:
    montage = unreal.load_asset(path)
    info[montage.get_name()] = {'length': montage.get_play_length()}
with open(OUT_DIR + '/gerpit_sources.json', 'w') as handle:
    json.dump(info, handle, indent=1)
unreal.log_warning('GERPIT_EXPORT info %s' % json.dumps(info))
