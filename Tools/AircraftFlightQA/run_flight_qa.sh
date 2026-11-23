#!/usr/bin/env bash
# Runs -AircraftFlightValidate headless (sharp turns, attack, idle, re-acquire on a Bf 110) and prints its checks.
# Needs the DebugGame editor build; nothing else may hold the DebugGame DLL or run UnrealBuildTool meanwhile.
# Usage: run_flight_qa.sh <label> [extra args, e.g. -AircraftLegacyFlight]
set -u
LABEL="${1:-flight}"; shift || true
PROJECT_DIR="/d/UE5Projects/RTS_Survival"
EDITOR="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
MAP="/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics"
OUT_DIR="$PROJECT_DIR/Saved/AircraftFlightQA"
mkdir -p "$OUT_DIR"
LOG="$OUT_DIR/$LABEL.log"
# Git Bash would rewrite /Game/... into a Windows path: no conversion, Windows paths for the rest.
MSYS_NO_PATHCONV=1 timeout 900 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" -game -unattended -nosound \
	-RenderOffscreen -windowed -ResX=1280 -ResY=720 -AircraftFlightValidate -AircraftFlightQALabel="$LABEL" \
	-AircraftTestAnchor=-14909,-9709 "$@" -log -abslog="$(cygpath -m "$LOG")" >/dev/null 2>&1
echo "== $LABEL (exit $?)"
grep -E "RTS_AIRCRAFT_TEST (CHECK|FLIGHT_METRICS|FLIGHT_STEP|RESULT|FLIGHT_CSV)|LogExit" "$LOG" | sed 's/^.*RTS_AIRCRAFT_TEST //' | head -80
