#!/bin/sh
# Runs a python file in a throwaway full editor (autosave off, no shared editor involved).
# usage: rubble_headless_run.sh <script.py> <map> <logfile> [timeout_s]
export MSYS_NO_PATHCONV=1
SCRIPT="$1"; MAP="$2"; LOG="$3"; LIMIT="${4:-400}"
mkdir -p "$(dirname "$LOG")"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject \
  "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=$MAP" \
  "-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False" \
  -ExecCmds="py $SCRIPT" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
