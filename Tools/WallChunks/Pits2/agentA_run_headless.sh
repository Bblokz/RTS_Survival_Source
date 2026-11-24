#!/bin/sh
# Agent A: run one throwaway Development editor with a python script under the shared headless.lock.
# usage: agentA_run_headless.sh <script.py> <logname> [timeout_s]
SCRIPT="$1"; LOGNAME="$2"; LIMIT="${3:-360}"
LOCK=D:/UE5Projects/RTS_Survival/Saved/WallChunks/headless.lock
until mkdir "$LOCK" 2>/dev/null; do echo "lock busy, waiting"; sleep 30; done
echo "agentA $(date)" > "$LOCK/owner.txt"
export MSYS_NO_PATHCONV=1
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle -ExecCmds="py $SCRIPT" -LogCmds="LogWallHoleDamage Verbose" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/$LOGNAME.log &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; kill $PID; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; break; fi
done
wait $PID 2>/dev/null
echo "exit after ${WAITED}s"
rm -rf "$LOCK"
