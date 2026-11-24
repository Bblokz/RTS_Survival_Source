#!/bin/sh
# Launches ambient_cull_test.py in a throwaway editor on RTS_ToCopySmall (never Ger_Berlin, never the shared editor).
# usage: ambient_run.sh <logname> [timeout_s]
# -DisablePlugins=Fab,BpGeneratorUltimate keeps it off the UECP broker, so other sessions' calls never land in it.
export MSYS_NO_PATHCONV=1
mkdir -p D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/cull_test
LOG=D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/cull_test/$1.log
LIMIT="${2:-300}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ambient/ambient_cull_test.py" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -ResX=1600 -ResY=900 -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing my own editor"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep "AMBCULL" "$LOG" | grep -v STATDUMP | sed 's/^\[[^]]*\]\[[^]]*\]//' | tail -${TAIL:-60}
