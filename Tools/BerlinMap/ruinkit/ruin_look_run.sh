#!/bin/sh
# Launches ruin_look_test.py in a throwaway editor on RTS_ToCopySmall (never Ger_Berlin, never the shared editor),
# then samples the colours of the top-down picture (ruin_look_colours.py).
# usage: ruin_look_run.sh <outname> [timeout_s]      pictures -> Saved/Berlin/ruinkit_look/<outname>/
export MSYS_NO_PATHCONV=1
export RUIN_LOOK_OUT="$1"
OUT=D:/UE5Projects/RTS_Survival/Saved/Berlin/ruinkit_look/$1
mkdir -p "$OUT"
LIMIT="${2:-420}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/ruin_look_test.py" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$OUT/editor.log &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep "RUINLOOK" "$OUT/editor.log" | cut -c1-300 | tail -n 30
python D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/ruin_look_colours.py "$OUT"
