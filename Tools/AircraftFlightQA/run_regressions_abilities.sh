#!/usr/bin/env bash
# Aircraft global-ability map tests whose flights go through the move/attack planner: carpet bombing (turnaround and
# second run), vehicle drops (approach, hover / landing hand-off, retreat) and the Ju 87 G tank busters (strafe passes).
set -u
EDITOR="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
OUT_DIR="/d/UE5Projects/RTS_Survival/Saved/AircraftFlightQA"
MAP="/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics"
mkdir -p "$OUT_DIR"
run_test() {
	local label="$1"; shift
	local log="$OUT_DIR/regression_$label.log"
	MSYS_NO_PATHCONV=1 timeout 900 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" -game -unattended \
		-nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 "$@" -log -abslog="$(cygpath -m "$log")" >/dev/null 2>&1
	echo "== $label (exit $?)"
	grep -E "(CHECK FAIL|RESULT|CARPET|Result)" "$log" | grep -v "PASS " | sed 's/^\[[^]]*\]\[[^]]*\]//' | cut -c1-240 | head -30
}
run_test carpet_bombing -CarpetBombingValidate
run_test aircraft_drop -AircraftDropValidate -AircraftTestAnchor=-14909,-9709
run_test ju87g -Ju87GTankBustersValidate -AircraftTestAnchor=-14909,-9709
