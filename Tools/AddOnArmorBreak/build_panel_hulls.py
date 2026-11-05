"""Builds the add-on armour panel hulls of every prepared vehicle (RTS.AddOnArmor.BuildPanelHull, see
Docs/Units/Vehicles/AddOnArmorPanels.md). Each target is <source>_AddOnPanels next to its source mesh, with its panel
materials (<material>_AddOnPanels) beside it; nothing a Blueprint uses is changed.

Run headless (new and rebuilt assets only; no editor may have the targets open):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -nullrhi -nosplash -nosound
    -DisablePlugins=Fab,BpGeneratorUltimate
Optional: set ONLY (comma-separated hull names) in the environment to build a subset; DRYRUN=1 classifies only.
Parameters were tuned on side views of each hull (panel geometry = islands at |Y| >= MinAbsY; see the command doc).
"""
import os

import unreal

ROOT = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle'

# Hull mesh -> (vehicle Blueprints using it, BuildPanelHull arguments).
HULLS = {
    'Ger_Light/PanzerII/SpacedArmor/SM_PZIIF_SpacedArmor_Hull': (
        ['Ger_Vehicle_PanzerII_F_SpacedArmor'], 'MinAbsY=114.9'),
    'Ger_Light/PanzerII_Luchs/Meshes/SM_Luchs_SpacedArmor_Hull': (
        ['Ger_Vehicle_PanzerII_Luchs_SpacedArmor'], 'MinAbsY=122.5'),
    'Ger_Medium/PanzerIII/SpacedArmor/SM_PanzerIIIM_SpacedArmor_Hull': (
        ['Ger_Vehicle_PanzerIII_M_SpacedArmor'], 'MinAbsY=173.5 MinPanelArea=10000'),
    # Pz IV family: 9 cm Schuerzen slabs; the long skirt rail stays hull.
    'Ger_Medium/PanzerIV/IV_H/StandardPlus/SM_Hull_PzIVH_StandardPlus': (
        ['Ger_Vehicle_PanzerIV_H_StandardPlus'], 'MinAbsY=154 MaxIslandLength=200'),
    'Ger_Medium/PanzerIV/CommanderMeshes/SM_PzIV_Commander_Hull': (
        ['Ger_Vehicle_PanzerIV_Commander'], 'MinAbsY=154 MaxIslandLength=200'),
    'Ger_Medium/Brumbar/Meshes/SM_Brummbar_Hull': (
        ['Ger_Vehicle_Brummbar', 'Ger_Vehicle_Brummbar_DirectSiege'], 'MinAbsY=154 MaxIslandLength=200'),
    'Ger_Medium/Brumbar/Meshes/SM_Hull_Brummbar_LateWar': (
        ['Ger_Vehicle_Brummbar_LateWar'], 'MinAbsY=154 MaxIslandLength=200'),
    # Double-faced plates (front and back face are separate islands) merge into one panel.
    'Ger_Heavy/E25/Assault/SM_Hull_E25_Assault': (
        ['Ger_Vehicle_E25_Assault'], 'MinAbsY=193 MinPanelArea=2000'),
    'Ger_Light/SDKFZ-140/Bunker/SM_Sdkfz140_Bunker_Hull': (
        ['Ger_Vehicle_SDKFZ-140_Bunker'], 'MinAbsY=117.5'),
    'Ger_Heavy/E75/Meshes/SM_E75_Hull': (
        ['Ger_Vehicle_E75'], 'MinAbsY=206'),
    # One skirt section per side is missing in the model; the left side's long hanger rail stays hull.
    'Ger_Heavy/PantherG/Meshes/SM_Hull_PantherG': (
        ['Ger_Vehicle_PantherG'], 'MinAbsY=156 MinPanelArea=10000 MaxIslandLength=200'),
    'Ger_Light/Hetzer/Mesh/SM_Hetzer_Camo_Hull': (
        ['Ger_Vehicle_Hetzer'], 'MinAbsY=136'),
    'Ger_Light/Hetzer/Flame/Meshes/SM_HetzerFlame_Camo_Hull': (
        ['Ger_Vehicle_Hetzer_Flame'], 'MinAbsY=136'),
    'Ger_Light/Hetzer/Mortar/Meshes/SM_HetzerMortar_Camo_Hull': (
        ['Ger_Vehicle_Hetzer_Mortar'], 'MinAbsY=136'),
    # Wire screens and cages: no large plates, so each side splits into sections; the Bollwerk's full-length frame
    # stays hull.
    'Ger_Heavy/JagdPanther/Bollwerk/Meshes/SM_JagdPanther_Bollwerk_Hull': (
        ['Ger_Vehicle_JagdPanther_Bollwerk'], 'MinAbsY=180 PanelsPerSide=4 MaxIslandLength=120'),
    'Ger_ArmoredCar/Sdkfz_234_3/StummelAssault/SM_Sdkfz234_3_StummelAssault_Hull': (
        ['Ger_ArmoredCar_Sdkfz_234_3_StummelAssault'], 'MinAbsY=137 PanelsPerSide=4'),
    'Ger_Heavy/SturmTiger/Scatterer/Meshes/SM_SturmTiger_Scatterer_Hull': (
        ['Ger_Vehicle_SturmTiger_Scatterer'], 'MinAbsY=209 PanelsPerSide=5'),
    # A separate bolted-on mesh (component "Armor" on TrackMesh), not the hull: the panels component covers the
    # plates of the registered mesh it hangs on.
    'Rus_Heavy/IS-1And-2/Meshes/SM_IS_1_SpacedArmor_Spiked': (
        ['Rus_Vehicle_IS-1_Harvest', 'Rus_Vehicle_IS-2_Harvest'], 'MinAbsY=120 PanelsPerSide=6'),
}

only = [name for name in os.environ.get('ONLY', '').split(',') if name]
dry_run = ' -DryRun' if os.environ.get('DRYRUN') == '1' else ''
for relative, (vehicles, arguments) in HULLS.items():
    if only and relative.split('/')[-1] not in only:
        continue
    source = f'{ROOT}/{relative}'
    command = f'RTS.AddOnArmor.BuildPanelHull Source={source} Target={source}_AddOnPanels {arguments}{dry_run}'
    unreal.log(f'RTS_ADDON_PANEL_BATCH {relative.split("/")[-1]} for {", ".join(vehicles)}')
    unreal.SystemLibrary.execute_console_command(None, command)
unreal.log('RTS_ADDON_PANEL_BATCH done')
