#!/usr/bin/env bash
# Headless Unreal python commandlet for the Ore mineral flora bushes, under the shared flora lock.
# usage: bash run_ue.sh <script.py> <logname> [script args...]
# Takes Saved/OreMountains/flora/ue.lock (mkdir; waits up to 30 min, shared with the trees builder), releases it after.
# Binary: DebugGame (BIN=Development for the Development DLL). Opens no map.
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/OreMineralFlora/bushes"
LOCK="D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/ue.lock"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
[ "${BIN:-DebugGame}" = "Development" ] && ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
SCRIPT="$1"; NAME="$2"; shift 2
LOG="D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/bushes/logs/$NAME.log"
mkdir -p "$(dirname "$LOG")"
WAITED=0
until mkdir "$LOCK" 2>/dev/null; do
  if [ $WAITED -ge 1800 ]; then echo "LOCK TIMEOUT after 30 min"; exit 4; fi
  sleep 20; WAITED=$((WAITED+20))
done
echo "bushes $NAME $(date)" > "$LOCK/owner.txt"
trap 'rm -rf "$LOCK"' EXIT
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/$SCRIPT $*" -unattended -nosplash -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -abslog="$LOG" > /dev/null 2>&1
RC=$?
echo "exit=$RC log=$LOG waited_for_lock=${WAITED}s"
grep -E "BUSH|Traceback|LogPython: Error|Error: .*\.py" "$LOG" | grep -v "LogLinker\|LogStreaming" | tail -40
exit $RC
