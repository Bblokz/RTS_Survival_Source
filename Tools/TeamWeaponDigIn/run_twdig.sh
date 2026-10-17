#!/bin/bash
# Runs the team weapon dig-in test (-TeamWeaponDigInValidate) on one map with the DebugGame editor.
# Usage: run_twdig.sh <label> <map> [extra test args...]
#   run_twdig.sh survey /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -TWDigInAt=16796,-15808 -TWDigInSurvey
#   run_twdig.sh flat /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -TWDigInAt=16796,-15808
# Pictures go to Saved/TWDigIn/<label>, the log to Saved/Logs/TWDigIn_<label>.log, the RTS_TWDIG lines to
# Saved/TWDigIn/<label>_summary.txt and a survey to Saved/TWDigIn/tw_digin_survey.json.
LABEL=$1; MAP=$2; shift 2
PROJECT_DIR=/d/UE5Projects/RTS_Survival
EDITOR=${RTS_EDITOR:-/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe}
cd "$PROJECT_DIR" || exit 1
rm -rf "Saved/TWDigIn/$LABEL"; mkdir -p "Saved/TWDigIn/$LABEL"
MSYS_NO_PATHCONV=1 timeout 2700 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" -game -unattended -nop4 \
  -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab -benchmark -fps=30 \
  "-ExecCmds=r.TextureStreaming 0" -TeamWeaponDigInValidate "-TWDigInDir=D:/UE5Projects/RTS_Survival/Saved/TWDigIn/$LABEL" \
  "$@" "-abslog=D:/UE5Projects/RTS_Survival/Saved/Logs/TWDigIn_$LABEL.log" > /dev/null 2>&1
echo "$LABEL exit $?"
grep "RTS_TWDIG" "Saved/Logs/TWDigIn_$LABEL.log" | grep -v " picture " \
  | sed 's/^.*LogRTSTeamWeaponDigIn: Display: //' > "Saved/TWDigIn/${LABEL}_summary.txt"
grep "RESULT\|check=FAIL" "Saved/TWDigIn/${LABEL}_summary.txt"
