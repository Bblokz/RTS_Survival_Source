#!/bin/sh
# Runs one python script of this folder in a headless Development commandlet (never the shared editor).
# usage: run_ue_py.sh <script.py> <log name> [extra args]
cd /d/UE5Projects/RTS_Survival || exit 1
mkdir -p Saved/OreMountains/radio
LOG="D:/UE5Projects/RTS_Survival/Saved/OreMountains/radio/$2.log"
EXE="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
S="$1"; shift 2
MSYS_NO_PATHCONV=1 "$EXE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="D:/UE5Projects/RTS_Survival/Source/Tools/OreRadioOutpost/$S" \
  -unattended -nosplash -NoSound -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate -abslog="$LOG" "$@" > /dev/null 2>&1
echo "exit $?  log $LOG"
grep -E "ORE_RADIO|Error: |Traceback|Error\]" "$LOG" | grep -v "LogInit" | sed 's/^.*LogPython: Warning: //' | cut -c1-600 | head -120
