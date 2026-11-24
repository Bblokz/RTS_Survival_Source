#!/bin/sh
# Runs one python script of this folder in a headless Development commandlet (never the shared editor).
# usage: run_ue_py.sh <script.py> <log name> [extra args, e.g. -nullrhi]
cd /d/UE5Projects/RTS_Survival || exit 1
mkdir -p Saved/OreMountains/sites/models
if tasklist | grep -i -E "UnrealEditor-Cmd|UnrealBuildTool" > /dev/null; then echo "BUSY: a commandlet or build is running"; exit 3; fi
LOG="D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/models/$2.log"
SCRIPT="$1"; shift 2
MSYS_NO_PATHCONV=1 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="D:/UE5Projects/RTS_Survival/Source/Tools/OreSites/$SCRIPT" \
  -unattended -nosplash -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="$LOG" "$@" > /dev/null 2>&1
echo "exit $?  log $LOG"
grep -E "ORE_HF|Error: |Traceback" "$LOG" | grep -v "LogInit" | sed 's/^.*LogPython: Warning: //' | cut -c1-900 | head -60
