#!/usr/bin/env bash
# Headless read-only render inventory (perf_inventory.py) of the perf-audit maps. Output Saved/PerfAudit/inventory_*.json
# Uses a real D3D12 RHI offscreen so material statistics come from the SM6 shaders the game uses.
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/PerfAudit"
MAPS="$*"
if [ -z "$MAPS" ]; then
  MAPS="/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission /Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission /Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01"
fi
if [ "${BIN:-Development}" = "DebugGame" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
fi
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/PerfAudit_Inventory.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/perf_inventory.py $MAPS" -unattended -nosplash -NoSound -RenderOffscreen -AllowCommandletRendering -DisablePlugins=Fab,BpGeneratorUltimate -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_PERF_INVENTORY" "$LOG" | sed 's/.*RTS_PERF_INVENTORY //'
