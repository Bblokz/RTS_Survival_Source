#!/bin/sh
# Launches furniture_headless_test.py in a throwaway editor on RTS_ToCopySmall (never Ger_Berlin, never the shared editor).
# usage: furniture_run.sh <logname> [timeout_s] [batch]     batch 1 = pieces 01-16 (default), 2 = pieces 17-32
export MSYS_NO_PATHCONV=1
mkdir -p D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture
LOG=D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/$1.log
LIMIT="${2:-420}"
export FURN_BATCH="${3:-1}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/furniture/furniture_headless_test.py" -LogCmds="LogWallHoleDamage Verbose, LogAudio VeryVerbose" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
