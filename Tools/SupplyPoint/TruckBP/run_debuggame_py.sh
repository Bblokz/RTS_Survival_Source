#!/bin/bash
# Runs a python script in the headless DebugGame commandlet under the shared DebugGame lock.
# Usage: run_debuggame_py.sh <script.py> <log name> [extra args...]
LOCK="D:/UE5Projects/RTS_Survival/Saved/AgentLocks/debuggame_build.lockdir"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
SCRIPT="$1"; LOGNAME="$2"; shift 2
LOG="D:/UE5Projects/RTS_Survival/Saved/SupplyTruckBP/$LOGNAME.log"
for i in $(seq 1 120); do
  if mkdir "$LOCK" 2>/dev/null; then GOT=1; break; fi
  sleep 5
done
if [ -z "$GOT" ]; then echo "LOCK_TIMEOUT"; exit 3; fi
trap 'rmdir "$LOCK"' EXIT
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript -script="$SCRIPT" \
  -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="$LOG" "$@" > /dev/null 2>&1
echo "EXIT $?  LOG $LOG"
