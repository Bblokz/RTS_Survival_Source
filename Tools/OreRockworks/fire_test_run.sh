#!/bin/sh
# Launches a rockwork test script in ONE throwaway editor on UnitTests/RTS_ToCopySmall (never a mission map, never the
# shared editor; the map is never saved). Kills only the editor it started, on timeout.
# usage: sh fire_test_run.sh <logname> [timeout_s]
#   default script rockwork_fire_test.py (ORW_ONLY=<rock>,<rock>); ROCK_SCRIPT=rockwork_crush_lineup_test.py for the
#   no-crush lanes + lineup pictures
export MSYS_NO_PATHCONV=1
OUTDIR=D:/UE5Projects/RTS_Survival/Saved/OreMountains/rockworks
mkdir -p $OUTDIR/fire $OUTDIR/lineup $OUTDIR/logs
LOG=$OUTDIR/logs/$1.log
LIMIT="${2:-600}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/OreRockworks/${ROCK_SCRIPT:-rockwork_fire_test.py}" -LogCmds="LogWallHoleDamage Verbose" -ResX=1280 -ResY=720 -WINDOWED -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing own editor"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep -E "ORWTEST|ORWLINE|RTS_WALL_HOLES" $LOG | cut -c1-600 | tail -120
