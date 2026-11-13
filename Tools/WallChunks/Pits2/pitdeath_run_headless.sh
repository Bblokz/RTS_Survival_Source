#!/bin/sh
# Pit death check: one throwaway DebugGame editor running pitdeath_test.py under the shared headless.lock.
# usage: pitdeath_run_headless.sh [timeout_s]
LIMIT="${1:-360}"
LOCK=D:/UE5Projects/RTS_Survival/Saved/WallChunks/headless.lock
SCRIPT=D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Pits2/pitdeath_test.py
until mkdir "$LOCK" 2>/dev/null; do echo "lock busy, waiting"; sleep 30; done
echo "pitdeath $(date)" > "$LOCK/owner.txt"
export MSYS_NO_PATHCONV=1
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle -ExecCmds="py $SCRIPT" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/pitdeath.log &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; kill $PID; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; break; fi
done
wait $PID 2>/dev/null
echo "exit after ${WAITED}s"
rm -rf "$LOCK"
