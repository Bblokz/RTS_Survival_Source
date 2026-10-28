#!/usr/bin/env bash
# Bakes SurfaceMap_<MapName> (ground surface per grid cell, from the landscape paint layers and the road/path meshes)
# for each map, headless. Bake again after repainting a landscape or moving roads.
# Usage: bake_surface_maps.sh [map package path...]   default: the German missions; BIN=Development for that DLL.
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/GroundSurface"
MAPS="$*"
if [ -z "$MAPS" ]; then
  MAPS="/Game/RTS_Survival/Maps/GerMissions/Ger_Trains /Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission"
  MAPS="$MAPS /Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35 /Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission"
  MAPS="$MAPS /Game/RTS_Survival/Maps/GerMissions/RedLine"
fi
if [ "${BIN:-DebugGame}" = "Development" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/GroundSurface_Bake.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/ground_surface_job.py bake $MAPS" -unattended -nosplash -NoSound -DisablePlugins=Fab -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_SURFACE_MAP|LogRTSSurfaceMapBake" "$LOG" | sed 's/.*RTS_SURFACE_MAP //'
echo "previews: D:/UE5Projects/RTS_Survival/Saved/GroundSurface/SurfaceMap_*.bmp"
