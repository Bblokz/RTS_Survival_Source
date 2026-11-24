#!/bin/sh
# Runs one python script in the headless Development commandlet (never the shared editor).
# usage: run_ue_py.sh <script.py> <log name>
cd /d/UE5Projects/RTS_Survival || exit 1
mkdir -p Saved/OderWarthe/stations
LOG="D:/UE5Projects/RTS_Survival/Saved/OderWarthe/stations/$2.log"
MSYS_NO_PATHCONV=1 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript -script="$1" \
  -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="$LOG" > /dev/null 2>&1
echo "exit $?  log $LOG"
grep -E "DFSTATION|DFICON|Error: |Traceback" "$LOG" | grep -v "LogInit" | sed 's/^.*LogPython: Warning: //' | head -80
