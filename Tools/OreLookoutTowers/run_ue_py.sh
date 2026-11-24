#!/bin/sh
# Runs one python script of this folder in a headless commandlet (never the shared editor).
# usage: run_ue_py.sh <script.py> <log name> [DebugGame]
# DebugGame: the BP build needs ALookoutTowerCaptureActor, which only the DebugGame DLL has until Development is rebuilt.
cd /d/UE5Projects/RTS_Survival || exit 1
mkdir -p Saved/OreMountains/lookout
LOG="D:/UE5Projects/RTS_Survival/Saved/OreMountains/lookout/$2.log"
EXE="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
[ "$3" = "DebugGame" ] && EXE="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
MSYS_NO_PATHCONV=1 "$EXE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="D:/UE5Projects/RTS_Survival/Source/Tools/OreLookoutTowers/$1" \
  -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="$LOG" > /dev/null 2>&1
echo "exit $?  log $LOG"
grep -E "LOOKOUT|Error: |Traceback" "$LOG" | grep -v "LogInit" | sed 's/^.*LogPython: Warning: //' | head -80
