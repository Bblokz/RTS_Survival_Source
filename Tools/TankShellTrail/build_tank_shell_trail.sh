#!/usr/bin/env bash
# Builds or re-patches NS_TankShellTrail (the tank shell tracer and smoke trail) in a headless DebugGame editor.
# Run after editing TankShellHlsl/*.hlsl; see Docs/VFX/TankShellTrail.md.
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/TankShellTrail"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/TankShellTrail_Build.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/tank_shell_trail_job.py" -unattended -nosplash -NoSound -DisablePlugins=Fab -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_MUZZLE_VFX_AUTHORING|RTS_TANK_SHELL_TRAIL_JOB|Error:.*(Niagara|MuzzleVfx|Python)" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -120
