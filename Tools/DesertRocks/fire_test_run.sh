#!/bin/sh
# Launches rock_fire_test.py in a throwaway editor on RTS_ToCopySmall (never a mission map, never the shared editor).
# usage: sh fire_test_run.sh <logname> [timeout_s]     (DROCK_ONLY=<L rock>,<M rock>; ROCK_SCRIPT=rock_lineup_shot.py for the lineup)
export MSYS_NO_PATHCONV=1
mkdir -p D:/UE5Projects/RTS_Survival/Saved/DesertRocks/test D:/UE5Projects/RTS_Survival/Saved/DesertRocks/lineup D:/UE5Projects/RTS_Survival/Saved/DesertRocks/crush
LOG=D:/UE5Projects/RTS_Survival/Saved/DesertRocks/test/$1.log
LIMIT="${2:-480}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/DesertRocks/${ROCK_SCRIPT:-rock_fire_test.py}" -LogCmds="LogWallHoleDamage Verbose" -ResX=1280 -ResY=720 -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing own editor"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep -E "DROCKTEST|DROCKLINE|CRUSHTEST|ROCKTEST|RTS_WALL_HOLES" $LOG | cut -c1-500 | tail -80
