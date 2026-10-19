#!/usr/bin/env bash
# One iteration of the target crosshair loop: generate the masks, texture previews, headless import into
# /Game/RTS_Survival/Materials/AimMaterials/Crosshairs, offscreen in-game captures and their contact sheets.
# Usage: iterate.sh [extra run_capture.sh switches...]     (WIRE/AT/CASES variables pass through to run_capture.sh)
# Needs the DebugGame editor target built; the import refuses to run while a capture still holds the packages.
set -eu
GENERATOR="D:/UE5Projects/RTS_Survival/PythonAutomation/TargetCrosshairs"
TOOLS="$(cd "$(dirname "$0")" && pwd)"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
python "$GENERATOR/generate_crosshairs.py"
python "$GENERATOR/preview_crosshairs.py"
SETUP_LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/AimCrosshairSetup.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$GENERATOR/unreal_setup_crosshairs.py assets" -unattended -nullrhi -DisablePlugins=Fab \
  -abslog="$SETUP_LOG" > /dev/null 2>&1 || true
if grep -qE "Traceback|LogPython: Error|ailed to save" "$SETUP_LOG"; then
  grep -E "Traceback|LogPython: Error|ailed to save" "$SETUP_LOG" | head
  exit 1
fi
grep -c "RTS_AIM_CROSSHAIR_SETUP" "$SETUP_LOG"
bash "$TOOLS/run_capture.sh" "D:/UE5Projects/RTS_Survival/Saved/AimCrosshairs" "$@"
python "$TOOLS/contact_sheet.py"
