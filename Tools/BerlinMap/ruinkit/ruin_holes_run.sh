#!/bin/sh
# Launches ruin_holes_headless_test.py in a throwaway editor on RTS_ToCopySmall (never Ger_Berlin, never the shared editor).
# usage: ruin_holes_run.sh <logname> [timeout_s]
export MSYS_NO_PATHCONV=1
LOG=D:/UE5Projects/RTS_Survival/Saved/Berlin/ruinkit_test/$1.log
LIMIT="${2:-480}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/ruin_holes_headless_test.py" -LogCmds="LogWallHoleDamage Verbose" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
