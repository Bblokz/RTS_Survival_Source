#!/usr/bin/env bash
# Runs mesh_reduce.py headless (commandlet with RHI for scene captures). Usage: run_mesh_reduce.sh <action> <tag> <mesh>=<value> ...
set -u
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/PerfAudit_MeshReduce.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="D:/UE5Projects/RTS_Survival/Source/Tools/PerfAudit/mesh_reduce.py $*" -unattended -nosplash -NoSound \
  -RenderOffscreen -AllowCommandletRendering -DisablePlugins=Fab,BpGeneratorUltimate -abslog="$LOG" > /dev/null 2>&1
echo "exit=$? log=$LOG"
grep -E "RTS_MESH_REDUCE|Error: " "$LOG" | grep -v "LogClass\|AutomationTest" | sed 's/.*RTS_MESH_REDUCE //' | tail -40
