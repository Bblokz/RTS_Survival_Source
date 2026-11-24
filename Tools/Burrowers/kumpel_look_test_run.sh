#!/bin/sh
# Launches kumpel_look_test.py in a throwaway editor on RTS_ToCopySmall (never the shared editor, map never saved).
# usage: kumpel_look_test_run.sh [timeout_s] [look|squad]    log + pictures -> Saved/Burrowers/look/
# Check VRAM first (nvidia-smi): a GPU reset crashes every Unreal process on the machine.
export MSYS_NO_PATHCONV=1
OUT=D:/UE5Projects/RTS_Survival/Saved/Burrowers/look
mkdir -p $OUT
LIMIT="${1:-600}"
TEST="${2:-look}"
[ "$TEST" = "squad" ] && SCRIPT=burrower_squad_test.py || SCRIPT=kumpel_look_test.py
[ "$TEST" = "squad" ] && OUT=D:/UE5Projects/RTS_Survival/Saved/Burrowers/squad && mkdir -p $OUT
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/Burrowers/$SCRIPT" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$OUT/$TEST.log &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
