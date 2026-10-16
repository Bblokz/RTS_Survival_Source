#!/bin/bash
# Runs the team weapon quality test (-TeamWeaponQualityValidate) on one map with the DebugGame editor.
# Usage: run_twqc.sh <label> <map> [extra test args...]
#   run_twqc.sh flat /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -TWQualityAt=16796,-15808
#   run_twqc.sh hills /Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission -TWQualitySearchCm=80000
# Pictures go to Saved/TWQuality/<label>, the log to Saved/Logs/TWQuality_<label>.log and the RTS_TWQC lines to
# Saved/TWQuality/<label>_summary.txt. Two maps can run in parallel.
LABEL=$1; MAP=$2; shift 2
PROJECT_DIR=/d/UE5Projects/RTS_Survival
EDITOR=/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe
cd "$PROJECT_DIR" || exit 1
rm -rf "Saved/TWQuality/$LABEL"; mkdir -p "Saved/TWQuality/$LABEL"
MSYS_NO_PATHCONV=1 timeout 2700 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" -game -unattended -nop4 \
  -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab -benchmark -fps=30 \
  "-ExecCmds=r.TextureStreaming 0" -TeamWeaponQualityValidate "-TWQualityDir=D:/UE5Projects/RTS_Survival/Saved/TWQuality/$LABEL" \
  "$@" "-abslog=D:/UE5Projects/RTS_Survival/Saved/Logs/TWQuality_$LABEL.log" > /dev/null 2>&1
echo "$LABEL exit $?"
grep "RTS_TWQC" "Saved/Logs/TWQuality_$LABEL.log" | grep -v " picture \|not on the navmesh" \
  | sed 's/^.*LogRTSTeamWeaponQuality: Display: //' | cut -c1-320 > "Saved/TWQuality/${LABEL}_summary.txt"
grep "RESULT\|hill_search\|check=FAIL" "Saved/TWQuality/${LABEL}_summary.txt"
