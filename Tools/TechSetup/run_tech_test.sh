#!/usr/bin/env bash
# Runs UTechBatchTwoTestSubsystem (-TechBatchTwoValidate) headless: spawns the affected units, researches the
# second batch of German technologies and checks every effect. Prints the CHECK lines and the RESULT line.
# Usage: run_tech_test.sh [extra UE switches...]     MAP=/Game/... picks another map (needs a navmesh).
# EXE=Development uses UnrealEditor-Cmd.exe instead of the DebugGame one.
set -u
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
ENGINE_DIR="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64"
if [ "${EXE:-DebugGame}" = "Development" ]; then
  ENGINE="$ENGINE_DIR/UnrealEditor-Cmd.exe"
else
  ENGINE="$ENGINE_DIR/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/TechBatchTwo.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -nullrhi -DisablePlugins=Fab -benchmark -fps=30 \
  -TechBatchTwoValidate -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "RTS_TECH_BATCH2 (CHECK|RESULT|research|baseline)" "$LOG"
