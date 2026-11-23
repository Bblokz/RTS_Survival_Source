#!/usr/bin/env bash
# Runs the existing aircraft map tests one after another on the current DebugGame build and prints their checks:
# strafing global ability (Bf 110), airbase take-off / landing, terrain following over the Ore Mountains ridge.
set -u
EDITOR="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
OUT_DIR="/d/UE5Projects/RTS_Survival/Saved/AircraftFlightQA"
mkdir -p "$OUT_DIR"
run_test() {
	local label="$1" map="$2"; shift 2
	local log="$OUT_DIR/regression_$label.log"
	MSYS_NO_PATHCONV=1 timeout 900 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$map" -game -unattended \
		-nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 "$@" -log -abslog="$(cygpath -m "$log")" >/dev/null 2>&1
	echo "== $label (exit $?)"
	grep -E "RTS_AIRCRAFT_TEST (CHECK FAIL|RESULT)" "$log" | sed 's/^.*RTS_AIRCRAFT_TEST //' | cut -c1-220 | head -30
}
run_test strafe /Game/RTS_Survival/Maps/UnitTests/UT_Nomadics -AircraftStrafeValidate \
	-AircraftStrafeAbility=GA_2xBf110Strafing -AircraftStrafeClassMarker=BF110Airborne -AircraftTestAnchor=-14909,-9709
run_test base_takeoff /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -AircraftBaseTakeOffValidate
run_test terrain_follow /Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains -AircraftTerrainFollowValidate \
	-AircraftTestAnchor=-4961,-14632
