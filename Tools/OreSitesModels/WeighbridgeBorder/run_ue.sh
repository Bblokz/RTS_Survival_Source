#!/bin/sh
# Runs one python script of this folder in a headless Development commandlet (never the shared editor, null RHI).
# usage: sh run_ue.sh <script.py> <log name>
cd /d/UE5Projects/RTS_Survival || exit 1
# wait (up to 10 min) while another commandlet or a build runs: other agents share this machine
for i in $(seq 1 60); do tasklist | grep -i -E "UnrealEditor-Cmd|UnrealEditor-Win64-DebugGame-Cmd|UnrealBuildTool" > /dev/null || break; sleep 10; done
mkdir -p Saved/OreMountains/sites/models
LOG="D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/models/ue_wbb_$2.log"
EXE="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
MSYS_NO_PATHCONV=1 "$EXE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="D:/UE5Projects/RTS_Survival/Source/Tools/OreSitesModels/WeighbridgeBorder/$1" \
  -unattended -nosplash -nullrhi -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="$LOG" > /dev/null 2>&1
echo "exit $?  log $LOG"
grep -E "ORE_SITES|Error: |Traceback|Assertion" "$LOG" | grep -v "LogInit" | sed 's/^.*LogPython: Warning: //' | cut -c1-600 | head -80
