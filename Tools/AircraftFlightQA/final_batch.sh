#!/usr/bin/env bash
# Final verification: build, flight QA, then the vehicle drop test.
bash /d/UE5Projects/RTS_Survival/Source/Tools/AircraftFlightQA/build_and_run.sh final 12 > /dev/null 2>&1
grep -E "RESULT|CHECK FAIL" /d/UE5Projects/RTS_Survival/Saved/AircraftFlightQA/final.log | sed 's/.*RTS_AIRCRAFT_TEST //'
EDITOR="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
LOG=/d/UE5Projects/RTS_Survival/Saved/AircraftFlightQA/regression_aircraft_drop.log
MSYS_NO_PATHCONV=1 timeout 900 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" /Game/RTS_Survival/Maps/UnitTests/UT_Nomadics -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -AircraftDropValidate -AircraftTestAnchor=-14909,-9709 -log -abslog="$(cygpath -m "$LOG")" >/dev/null 2>&1
echo "== aircraft_drop (exit $?)"
grep -E "RTS_AIRCRAFT_TEST (CHECK|RESULT)" "$LOG" | sed 's/^.*RTS_AIRCRAFT_TEST //' | cut -c1-200 | tail -30
