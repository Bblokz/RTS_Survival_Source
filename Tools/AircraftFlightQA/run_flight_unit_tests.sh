#!/usr/bin/env bash
# Runs the pure-math aircraft automation tests (flight model + terrain following) headless in the DebugGame editor.
set -u
PROJECT_DIR="/d/UE5Projects/RTS_Survival"
EDITOR="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
OUT_DIR="$PROJECT_DIR/Saved/AircraftFlightQA"
mkdir -p "$OUT_DIR"
LOG="$OUT_DIR/unit_tests.log"
MSYS_NO_PATHCONV=1 timeout 900 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" \
	-ExecCmds="Automation RunTests RTS.Aircraft.FlightModel+RTS.Aircraft.TerrainFollow; Quit" \
	-unattended -nullrhi -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate \
	-log -abslog="$(cygpath -m "$LOG")" >/dev/null 2>&1
echo "== unit tests (exit $?)"
grep -E "Test Completed|Result=|Error:.*RTS\.Aircraft|Expected|TestTrue|Failed" "$LOG" | grep -v "^.*LogPython" | head -60
