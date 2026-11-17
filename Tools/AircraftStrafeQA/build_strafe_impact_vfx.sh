#!/usr/bin/env bash
# Builds or re-patches NS_AircraftStrafeImpacts in a headless DebugGame editor (Docs/Units/Aircraft/AircraftStrafeGun.md).
# No open editor may hold the system: close its tab or unload the package first, or the save fails (error 32).
#   build_strafe_impact_vfx.sh [build|patch|rebuild]      default build; rebuild deletes the system first
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/AircraftStrafeQA"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/StrafeImpactVfx_Build.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/strafe_impact_vfx_job.py ${1:-build}" -unattended -nosplash -NoSound -DisablePlugins=Fab -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_STRAFE_IMPACT_(AUTHORING|JOB)|Error:.*(Niagara|StrafeImpact|Python)" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -80
