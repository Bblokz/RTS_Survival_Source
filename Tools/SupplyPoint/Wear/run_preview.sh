#!/bin/bash
# Runs ue_preview_wear.py in a throwaway Development editor (the shared editor is never touched). Usage: run_preview.sh [tag]
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/SupplyPointWear/preview${1:+_$1}.log"
export WEAR_TAG="${1:+_$1}"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" \
  "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" \
  -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Wear/ue_preview_wear.py" \
  -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -ResX=1280 -ResY=720 -abslog="$LOG" > /dev/null 2>&1
echo "EXIT $?"
grep -a "WEARPREVIEW\|Failed to compile\|error X\|\[SM6\] .*error\|LogPython: Error" "$LOG" | sed 's/^.*\(WEARPREVIEW\)/\1/' | head -40
