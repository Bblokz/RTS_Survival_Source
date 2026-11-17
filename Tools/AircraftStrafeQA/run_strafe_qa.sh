#!/usr/bin/env bash
# Runs -AircraftStrafeValidate headless twice (legacy MG traces, then the strafe gun) and plots both.
# Needs the DebugGame editor build; nothing else may hold the DebugGame DLL or run UnrealBuildTool meanwhile.
# Usage: run_strafe_qa.sh [both|legacy|new]
set -u
MODE="${1:-both}"
PROJECT_DIR="/d/UE5Projects/RTS_Survival"
EDITOR="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
MAP="/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics"
OUT_DIR="$PROJECT_DIR/Saved/AircraftStrafeQA"
OUT_DIR_WIN="$(cygpath -m "$OUT_DIR")"
mkdir -p "$OUT_DIR"

run_one() {
	local label="$1"; shift
	local log="$OUT_DIR/$label.log"
	# Git Bash would rewrite /Game/... into a Windows path: no conversion, Windows paths for the rest.
	MSYS_NO_PATHCONV=1 timeout 600 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" -game -unattended -nosound -RenderOffscreen \
		-windowed -ResX=1280 -ResY=720 -AircraftStrafeValidate -AircraftStrafeAbility=GA_2xBf110Strafing -AircraftStrafeClassMarker=BF110Airborne "$@" -log -abslog="$(cygpath -m "$log")" >/dev/null 2>&1
	echo "== $label (exit $?)"
	grep -E "RTS_AIRCRAFT_TEST (CHECK|STRAFE_QA|SUMMARY|RESULT)|Pool saturated|LogExit" "$log" | head -40
}

if [ "$MODE" = "both" ] || [ "$MODE" = "legacy" ]; then
	run_one strafe_legacy_guns -AircraftStrafeLegacyGuns
fi
if [ "$MODE" = "both" ] || [ "$MODE" = "new" ]; then
	run_one strafe_gun
fi
python "$(cygpath -m "$PROJECT_DIR")/Source/Tools/AircraftStrafeQA/plot_strafe_qa.py" "$OUT_DIR_WIN/strafe_qa_compare.png" 	"legacy MG traces=$OUT_DIR_WIN/strafe_legacy_guns.csv" "strafe gun=$OUT_DIR_WIN/strafe_gun.csv"
