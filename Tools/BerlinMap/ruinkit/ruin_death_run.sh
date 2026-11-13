#!/bin/sh
# Launches ruin_death_headless_test.py in a throwaway editor on RTS_ToCopySmall (never Ger_Berlin, never the shared editor).
# usage: ruin_death_run.sh <logname> [timeout_s] [fx]   log -> Saved/Berlin/ruin_death_test/<logname>.log
# third argument "fx": run the effect contact sheet (ruin_death_fx_probe.py) instead of the test
# third argument "large2": the test on the second set (B7, E2, B5, B4; result_large2.json)
export MSYS_NO_PATHCONV=1
mkdir -p D:/UE5Projects/RTS_Survival/Saved/Berlin/ruin_death_test
LOG=D:/UE5Projects/RTS_Survival/Saved/Berlin/ruin_death_test/$1.log
LIMIT="${2:-660}"
SCRIPT=ruin_death_headless_test.py
if [ "$3" = "fx" ]; then SCRIPT=ruin_death_fx_probe.py; fi
if [ "$3" = "large2" ]; then export RUIN_DEATH_SET=large2; fi
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/$SCRIPT" -LogCmds="LogWallHoleDamage Verbose, LogRTSCoverFinder Verbose" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
