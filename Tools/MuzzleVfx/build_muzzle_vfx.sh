#!/usr/bin/env bash
# Builds or re-patches NS_SmallArmsMuzzle in a headless editor (Docs/VFX/SmallArmsMuzzleVfx.md). An open editor
# must not hold the system or its materials: close their tabs or unload the packages first, or the save fails (error 32).
#   build_muzzle_vfx.sh [build|patch]      default build
#   BIN=Development build_muzzle_vfx.sh    default DebugGame (the shared editor holds the Development DLL)
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/MuzzleVfx"
if [ "${BIN:-DebugGame}" = "Development" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/MuzzleVfx_Build.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/muzzle_vfx_job.py ${1:-build}" -unattended -nosplash -NoSound -DisablePlugins=Fab -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_MUZZLE_VFX_(AUTHORING|JOB)|Error:.*(Niagara|MuzzleVfx|Python)" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -80
