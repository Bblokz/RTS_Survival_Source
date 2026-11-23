#!/usr/bin/env bash
# Builds the DebugGame editor, retrying while other sessions' half-written files break the shared tree, then runs
# the flight QA. Gives up when an error is in an aircraft file (that one is ours to fix).
# Usage: build_and_run.sh <label> [max attempts] [extra QA args]
set -u
LABEL="${1:-flight}"; ATTEMPTS="${2:-12}"; shift 2 || shift $#
SP="/c/Users/basbl/AppData/Local/Temp/claude/71fd5df64096b71c/7b9393dd-98cb-4935-9b01-f7315ed17132/scratchpad"
LOG="$SP/build_$LABEL.log"
for attempt in $(seq 1 "$ATTEMPTS"); do
	powershell.exe -NoProfile -Command "& 'D:\\UnrealEngine\\UE5.5_ReleaseBranch\\UnrealEngine\\Engine\\Build\\BatchFiles\\Build.bat' RTS_SurvivalEditor Win64 DebugGame '-Project=D:\\UE5Projects\\RTS_Survival\\RTS_Survival.uproject' -WaitMutex -NoHotReloadFromIDE *> '$(cygpath -w "$LOG")'"
	# PowerShell writes the log as UTF-16.
	ERRORS=$(iconv -f UTF-16 -t UTF-8 "$LOG" 2>/dev/null | grep -E "error C[0-9]|: error|error LNK" | sort -u)
	if [ -z "$ERRORS" ]; then
		echo "== build ok (attempt $attempt)"
		bash /d/UE5Projects/RTS_Survival/Source/Tools/AircraftFlightQA/run_flight_qa.sh "$LABEL" "$@" | grep -v "FLIGHT_STEP\|CHECK PASS" | cut -c1-230
		exit 0
	fi
	if echo "$ERRORS" | grep -qi "Aircraft"; then
		echo "== aircraft compile error:"; echo "$ERRORS" | head -20; exit 2
	fi
	echo "== attempt $attempt: foreign errors: $(echo "$ERRORS" | head -2 | cut -c1-160)"
	sleep 180
done
echo "== gave up"; exit 3
