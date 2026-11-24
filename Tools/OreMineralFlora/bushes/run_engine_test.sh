#!/bin/sh
# Launches bush_engine_test.py in ONE small throwaway editor on UnitTests/RTS_ToCopySmall (never a game map, never a shared
# editor). Takes the shared flora lock (Saved/OreMountains/flora/ue.lock), refuses below 4 GB free video memory, kills only
# its own process on timeout, never saves the map. usage: sh run_engine_test.sh <logname> [timeout_s]
export MSYS_NO_PATHCONV=1
HERE=D:/UE5Projects/RTS_Survival/Source/Tools/OreMineralFlora/bushes
LOCK=D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/ue.lock
mkdir -p D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/bushes/test D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/bushes/rev2/test
LOG=D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/bushes/test/$1.log
LIMIT="${2:-600}"
W=0
until mkdir "$LOCK" 2>/dev/null; do
  if [ $W -ge 1800 ]; then echo "LOCK TIMEOUT"; exit 4; fi
  sleep 20; W=$((W+20))
done
echo "bushes engine test $1 $(date)" > "$LOCK/owner.txt"
trap 'rm -rf "$LOCK"' EXIT
FREE=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits | head -1 | tr -d ' ')
echo "free video memory ${FREE} MiB, waited ${W}s for the lock"
if [ "${FREE:-0}" -lt 4000 ]; then echo "REFUSED: under 4 GB free video memory"; exit 5; fi
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py $HERE/bush_engine_test.py" -ResX=960 -ResY=540 -WINDOWED -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing own editor"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep -E "BUSHTEST" $LOG | cut -c1-700 | tail -70
