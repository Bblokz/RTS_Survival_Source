#!/bin/sh
# Runs one python script of this folder in a headless Development commandlet (never the shared editor).
# usage: run_ue_py.sh <script.py> <log name> [render]
#   render: adds -AllowCommandletRendering instead of -nullrhi (scene captures; check nvidia-smi first)
cd /d/UE5Projects/RTS_Survival || exit 1
mkdir -p Saved/OreMountains/sites/ue_oreplant
LOG="D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/ue_oreplant/$2.log"
EXE="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
RHI="-nullrhi"
[ "$3" = "render" ] && RHI="-AllowCommandletRendering"
MSYS_NO_PATHCONV=1 "$EXE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="D:/UE5Projects/RTS_Survival/Source/Tools/OreSitesModels/OrePlantHeapStation/$1" \
  -unattended -nosplash -NoSound $RHI -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="$LOG" > /dev/null 2>&1
echo "exit $?  log $LOG"
grep -E "OREPLANT|Error: |Traceback" "$LOG" | grep -v "LogInit" | sed 's/^.*LogPython: Warning: //' | head -80
