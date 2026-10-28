#!/usr/bin/env bash
# Bakes SandMask_<MapName> (which grid cells are sand, from the landscape paint layers) for each map, headless.
# Bake again after repainting a landscape. Usage: bake_sand_masks.sh [map package path...]
#   default: Ger_Trains and Ger_FirstMission; BIN=Development as in patch_dust_devils.sh
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/AmbientFx"
MAPS="$*"
if [ -z "$MAPS" ]; then
  MAPS="/Game/RTS_Survival/Maps/GerMissions/Ger_Trains /Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission"
fi
if [ "${BIN:-DebugGame}" = "Development" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/AmbientFx_Bake.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/ambient_fx_job.py bake $MAPS" -unattended -nosplash -NoSound -DisablePlugins=Fab -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_SAND_MASK" "$LOG" | sed 's/.*RTS_SAND_MASK //'
echo "previews: D:/UE5Projects/RTS_Survival/Saved/AmbientFx/SandMask_*.bmp"
