"""Exports precedent BXP Blueprints to T3D (read-only) into Saved/SDBxpRound2/t3d/ for graph inspection."""
import os
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/SDBxpRound2/t3d/'
BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
NOMADIC = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/'
PATHS = [
    BXP + 'GerBxps/Forge/SchreckFrame/Ger_Bxp_LFactorySchreckFrame',
    NOMADIC + 'GerT2/CommunicationsCenter/BXPs/CipherRoom/Ger_Bxp_CommCipherRoom',
    '/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets/RusBasic/Rus_EIT_BasicSoviet_Mosin',
    '/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/Kar98/Ger_Weapon_Kar98Sniper',
]
os.makedirs(OUT, exist_ok=True)
for path in PATHS:
    asset = unreal.load_asset(path)
    if asset is None:
        unreal.log_warning('T3D missing ' + path)
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = OUT + path.split('/')[-1] + '.t3d'
    task.exporter = unreal.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log('T3D %s %s' % (path, ok))
reg = unreal.AssetRegistryHelpers.get_asset_registry()
with open(OUT + 'listing.txt', 'w', encoding='utf-8') as f:
    for folder in ('/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets',
                   NOMADIC + 'GerT2/CommunicationsCenter/BXPs/CipherRoom',
                   '/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/SniperTeam',
                   '/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/Kar98'):
        for a in reg.get_assets_by_path(folder, recursive=True):
            f.write('%s %s\n' % (a.package_name, a.asset_class_path.asset_name))
