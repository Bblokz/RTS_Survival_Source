#!/usr/bin/env bash
# Headless Unreal python run for the desert rocks (commandlet with a renderer; never the shared editor).
# usage: bash run_ue.sh <script.py ["args"]> <logname> [extra args]
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/DesertRocks"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
SCRIPT="$1"; NAME="$2"; shift 2
LOG="D:/UE5Projects/RTS_Survival/Saved/DesertRocks/logs/$NAME.log"
mkdir -p "$(dirname "$LOG")"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/$SCRIPT" -unattended -nosplash -NoSound -AllowCommandletRendering \
  -DisablePlugins=Fab,BpGeneratorUltimate -abslog="$LOG" "$@" > /dev/null 2>&1
echo "exit=$? log=$LOG"
grep -E "DROCK_|WALL_HOLES|Traceback|LogPython: Error|Error: " "$LOG" | grep -v "LogLinker\|LogStreaming" | cut -c1-400 | tail -40
