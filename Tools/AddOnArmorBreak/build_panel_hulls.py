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

SU_SKIRT_ARGS = (
    'MinAbsY=157 MinZ=13 MaxZ=98 MinPanelArea=9500 MaxIslandLength=110 '
    f'DebrisMesh={ROOT}/Rus_Medium/SU_SharedArmor/SM_SU85_SU100_SpacedArmor_AddOnPanels_Debris'
)

# Panther D chassis family: the Jaguar and Jaguar Flanker hulls carry the same Schuerzen plates, so all three throw
# the Panther D's debris plate. It is cut from panel 5, the front left plate: the middle plate (the cut-out default)
# carries the faction sigil stamp, which Niagara would draw on every flying plate. Build SM_PantherD first.
PANTHERD_DEBRIS = f'{ROOT}/Ger_Heavy/PantherD/Meshes/SM_PantherD_AddOnPanels_Debris'
JAGUAR_SKIRT_ARGS = f'MinAbsY=148 MinZ=80 MaxZ=145 MinPanelArea=9000 MaxIslandLength=110 DebrisMesh={PANTHERD_DEBRIS}'

IS3_PANEL_ARGS ='MinAbsY=193 MinZ=57 MaxZ=149.5 MaxIslandLength=160 MinPanelArea=8500'

T34E_SKIRT_ARGS = 'MinAbsY=146 MinZ=30 MaxZ=118 MinPanelArea=4000 MaxIslandLength=220 SplitX=-192/-121/-48/25/96'

PZIII_FENCE_ARGS ='MinAbsY=150.5 MaxZ=142 PanelsPerSide=5 MaxIslandLength=95'
IS3_FOLDER = f'{ROOT}/Rus_Heavy/IS-1And-2/SpacedArmor_IS3'

# Hull mesh -> (vehicle Blueprints using it, BuildPanelHull arguments).
HULLS = {
    # MinAbsY keeps the hanger brackets (|Y| from 162.3) on the hull; MaxZ keeps the stowage bins out.
    'Ger_Heavy/PantherD/Meshes/SM_PantherD': (
        ['Ger_Vehicle_PantherD'],
        f'MinAbsY=163.5 MinZ=95 MaxZ=160 MinPanelArea=9000 MaxIslandLength=110 DebrisMesh={PANTHERD_DEBRIS}'),
    'Ger_Medium/Jaguar/Meshes/SM_Ger_Jaguar_Hull': (
        ['Ger_Vehicle_Jaguar'], JAGUAR_SKIRT_ARGS),
    'Ger_Medium/Jaguar/Flanker/SM_Jaguar_Flanker_Hull': (
        ['Ger_Vehicle_Jaguar_Flanker'], JAGUAR_SKIRT_ARGS),
    'Rus_Heavy/IS-1And-2/SpacedArmor_IS3/SM_IS3_Standard_FacetedSkirts_Hull': (
        ['Rus_Vehicle_IS-3'],
        f'{IS3_PANEL_ARGS} DebrisMesh={IS3_FOLDER}/SM_IS3_Standard_FacetedSkirts_Hull_AddOnPanels_Debris'),
    'Rus_Heavy/IS-1And-2/SpacedArmor_IS3/SM_IS3_Soviet_FacetedSkirts_Hull': (
        ['Rus_Vehicle_IS-3_Soviet'],
        f'{IS3_PANEL_ARGS} DebrisMesh={IS3_FOLDER}/SM_IS3_Soviet_FacetedSkirts_Hull_AddOnPanels_Debris'),
    'Rus_Heavy/IS-1And-2/SpacedArmor_IS3/SM_IS3_Flame_FacetedSkirts_Hull': (
        ['Rus_Vehicle_IS-3_Flame'],
        f'{IS3_PANEL_ARGS} DebrisMesh={IS3_FOLDER}/SM_IS3_Flame_FacetedSkirts_Hull_AddOnPanels_Debris'),
    'Rus_Heavy/IS-1And-2/HEAT_SpacedArmor/SM_IS1_HEAT_CurvedSkirts_Hull': (
        ['Rus_Vehicle_IS-1_Soviet'],
        'MinAbsY=159 MinZ=58 MaxZ=125 MaxIslandLength=151 MinPanelArea=9000 '
        f'DebrisMesh={ROOT}/Rus_Heavy/IS-1And-2/HEAT_SpacedArmor/SM_IS1_HEAT_CurvedSkirts_Hull_AddOnPanels_Debris'),
    'Rus_Light/T26/SpacedArmor/BakedHull/SM_T26_Soviet_SpacedArmor_Hull': (
        ['Rus_Vehicle_T26_Soviet', 'Rus_Vehicle_T26_Soviet_SpacedArmor'],
        'MinAbsY=130 MinZ=45 MaxZ=127 MaxIslandLength=125 MinPanelArea=5500 '
        f'DebrisMesh={ROOT}/Rus_Light/T26/SpacedArmor/BakedHull/SM_T26_Soviet_SpacedArmor_Hull_AddOnPanels_Debris'),
    'Rus_Heavy/KV1/Siege/SM_KV1_Siege_Hull': (
        ['Rus_Vehicle_KV1_L11_Soviet_Siege'],
        'MinAbsY=170.4 MinZ=45 MaxZ=131 MinPanelArea=6000 MaxIslandLength=135 '
        f'DebrisMesh={ROOT}/Rus_Heavy/KV1/Siege/SM_KV1_Siege_Hull_AddOnPanels_Debris'),
    'Rus_Medium/SU85/Meshes/SM_SU85_Soviet_SpacedArmor_Hull': (
        ['Rus_Vehicle_Su85_Soviet'], SU_SKIRT_ARGS),
    'Rus_Medium/Su100/Meshes/SM_SU100_Soviet_SpacedArmor_Hull': (
        ['Rus_Vehicle_Su100_Soviet'], SU_SKIRT_ARGS),
    # Soviet BA-11 / BA-14 (shared hull): two fender skirts over the rear wheels (|Y| 121.5) and one belly plate
    # (|Y| 100) per side. MinAbsY 99 keeps the spare wheels (|Y| from 86.2), the fender L-brackets and the belly
    # standoffs (both reach in past |Y| 99) on the hull. Debris = panel 1, the left front skirt.
    'Rus_ArmoredCar/BA-11/SovietArmor/SM_Ba11_Soviet_SpacedArmor_Hull': (
        ['Rus_ArmoredCar_BA11_Soviet', 'Rus_ArmoredCar_BA14_Soviet'],
        'MinAbsY=99 MinZ=35 MaxZ=110 MinPanelArea=4000 MaxIslandLength=110 '
        f'MaterialOverrides=0:{ROOT}/Rus_ArmoredCar/BA-11/Materials/MI_BAHull_Soviet '
        f'DebrisMesh={ROOT}/Rus_ArmoredCar/BA-11/SovietArmor/SM_Ba11_Soviet_SpacedArmor_Hull_AddOnPanels_Debris'),
    'Ger_Light/PanzerII/SpacedArmor/SM_PZIIF_SpacedArmor_Hull': (
        ['Ger_Vehicle_PanzerII_F_SpacedArmor'], 'MinAbsY=114.9'),
    # Strike Division wire-mesh grille fences over the tracks (base Pz II F, Radixite, Strike): one section per fence.
    'Ger_Light/PanzerII/StrikeFences/SM_PZIIF_Fenced_Hull': (
        ['Ger_Vehicle_PanzerII_F', 'Ger_Vehicle_PanzerII_F_RadixiteAutocannon', 'Ger_Vehicle_PanzerII_F_Strike'],
        'MinAbsY=114.5 MinZ=35 MaxZ=125 PanelsPerSide=5'),
    'Ger_Light/PanzerII_Luchs/Meshes/SM_Luchs_SpacedArmor_Hull': (
        ['Ger_Vehicle_PanzerII_Luchs_SpacedArmor'], 'MinAbsY=122.5'),
    'Ger_Medium/PanzerIII/SpacedArmor/SM_PanzerIIIM_SpacedArmor_Hull': (
        ['Ger_Vehicle_PanzerIII_M_SpacedArmor'], 'MinAbsY=173.5 MinPanelArea=10000'),
    # Pz III Strike Division fences (2026-11-07, Blender: Variants/PanzerIII_SDFence): 5 self-contained wire-mesh
    # sections of 91.2 cm per side with 4 cm gaps, every part at |Y| >= 151.2; the guard clamps stay hull.
    'Ger_Medium/PanzerIII/Meshes/SM_PanzerIII_SDFence_Hull': (
        ['Ger_Vehicle_PanzerIII_J', 'Ger_Vehicle_PanzerIII_AA'], PZIII_FENCE_ARGS),
    'Ger_Medium/PanzerIII/Flak41/SM_PzIIIAA_Flak41_SDFence_Hull': (
        ['Ger_Vehicle_PanzerIII_AA_Flak41'], PZIII_FENCE_ARGS),
    'Ger_Medium/PanzerIII/CommandMeshes/SM_PZIII_Commander_SDFence_Hull': (
        ['Ger_Vehicle_PanzerIII_J_Commander'], PZIII_FENCE_ARGS),
    # Panzerjaeger I Laser Strike Division fences (2026-11-10, Blender: Variants/PanzerjagerLaser_SDFence): the ribbed
    # post-apo side plates are gone; 5 self-contained wire-mesh sections per side, every part at |Y| >= 100.7; the
    # fender clamps stay hull (|Y| <= 99.6).
    'Ger_Light/PanzerJager1/Meshes/SM_PanzerJager_LaserAmmo_SDFence_Hull': (
        ['Ger_Vehicle_PanzerJager_Laser'], 'MinAbsY=100 MinZ=20 MaxZ=94 PanelsPerSide=5 MaxIslandLength=72'),
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
    # E-25 and E-25 Hunter: the Jaguar plate set baked on by bake_e25_skirts.py (run that first; it builds these too).
    'Ger_Heavy/E25/HullAndTurret/SM_E25_Hull_Skirts': (
        ['Ger_Vehicle_E25'],
        f'MinAbsY=193.5 MinZ=85 MaxZ=150 MinPanelArea=8000 MaxIslandLength=110 DebrisMesh={PANTHERD_DEBRIS}'),
    'Ger_Heavy/E25/Hunter/SM_E25_Hunter_Hull_Skirts': (
        ['Ger_Vehicle_E25_Hunter'],
        f'MinAbsY=193.5 MinZ=85 MaxZ=150 MinPanelArea=8000 MaxIslandLength=110 DebrisMesh={PANTHERD_DEBRIS}'),
    'Ger_Light/SDKFZ-140/Bunker/SM_Sdkfz140_Bunker_Hull': (
        ['Ger_Vehicle_SDKFZ-140_Bunker'], 'MinAbsY=117.5'),
    'Ger_Heavy/E75/Meshes/SM_E75_Hull': (
        ['Ger_Vehicle_E75'], 'MinAbsY=206'),
    # Panther II Strike Division hull (2026-11-07, Blender: Variants/PantherII_StrikeFences): the 4 Schuerzen plates per side
    # were cut apart in Blender; the hangers (|Y| 115-127) stay hull. The wire fences on the angled side armour sit above
    # MaxZ (every fence part z >= 81.9) and stay hull too. The BP scales the hull component (1.30, 1.25, 1.25).
    'Ger_Heavy/PantherII/StrikeDivision/SM_PantherII_Strike_Hull': (
        ['Ger_Vehicle_PantherII'], 'MinAbsY=125 MinZ=0 MaxZ=81 MinPanelArea=5000 MaxIslandLength=112'),
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
    # T-34E: the skirt is one piece per side; SplitX cuts it at the five rivet seams into 6 plates per side (rear,
    # 4 middle, front). The left front plate takes the small handle on top (MinAbsY 146). Debris: shared middle plate
    # (panel 2) plus the 4 end plates as their own meshes (ExtractPanelDebris -OwnPanel, see AddOnArmorPanels.md).
    'Rus_Medium/T34/Meshes/SM_T-34E_Hull': (
        ['Rus_Vehicle_T34E'], T34E_SKIRT_ARGS),
}

# Second panel hulls built from a source above, for Blueprints that override the hull material (each camo throws
# plates in its own material): target -> (source, vehicle Blueprints, BuildPanelHull arguments).
ALTERNATE_TARGETS = {
    'Rus_Medium/T34/Meshes/SM_T-34E_Hull_Soviet_AddOnPanels': (
        'Rus_Medium/T34/Meshes/SM_T-34E_Hull', ['Rus_Vehicle_T34E_Soviet'],
        f'{T34E_SKIRT_ARGS} MaterialOverrides=0:{ROOT}/Rus_Medium/T34/Meshes/MI_T34E_Soviet'),
}

only = [name for name in os.environ.get('ONLY', '').split(',') if name]
dry_run = ' -DryRun' if os.environ.get('DRYRUN') == '1' else ''
builds = [(f'{relative}_AddOnPanels', relative, vehicles, arguments)
          for relative, (vehicles, arguments) in HULLS.items()]
builds += [(target, source, vehicles, arguments)
           for target, (source, vehicles, arguments) in ALTERNATE_TARGETS.items()]
for target, relative, vehicles, arguments in builds:
    if only and relative.split('/')[-1] not in only and target.split('/')[-1] not in only:
        continue
    command = (f'RTS.AddOnArmor.BuildPanelHull Source={ROOT}/{relative} Target={ROOT}/{target} '
               f'{arguments}{dry_run}')
    unreal.log(f'RTS_ADDON_PANEL_BATCH {target.split("/")[-1]} for {", ".join(vehicles)}')
    unreal.SystemLibrary.execute_console_command(None, command)
unreal.log('RTS_ADDON_PANEL_BATCH done')
