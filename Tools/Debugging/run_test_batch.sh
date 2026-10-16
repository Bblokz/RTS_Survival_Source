#!/bin/bash
# Runs a list of packaged test scenarios under lldb (run_under_lldb.sh) and writes one summary line per run:
# debugger breaks / exceptions / crash, how the process ended, and the scenario's RESULT line(s).
#
# Usage: run_test_batch.sh <child exe> <log dir> <batch file>
# Batch file lines: name|timeout seconds|game arguments (space separated, no quotes); '#' starts a comment.
#   field_defense|1800|/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics -FieldDefenseValidate -benchmark -fps=30
# Every run also gets: -unattended -nosound <RTS_BATCH_RHI> -windowed -ResX/-ResY -ABSLOG=<log dir>/game_<name>.log
# and, through -DPCVars, low scalability so a run fits on an integrated GPU (merged into a scenario's own -DPCVars).
# Environment: RTS_BATCH_LANES (runs at the same time, default 1), RTS_BATCH_RESX / RTS_BATCH_RESY (default 640x360),
# RTS_BATCH_CVARS (the low scalability cvars; empty for full quality), RTS_BATCH_RHI (default -RenderOffscreen;
# -NullRHI runs game logic without drawing, much faster on a weak GPU, but the fog of war does not start).
set -u
Executable="$1"
LogDirectory="$2"
BatchFile="$3"
Lanes="${RTS_BATCH_LANES:-1}"
ResX="${RTS_BATCH_RESX:-640}"
ResY="${RTS_BATCH_RESY:-360}"
RhiArgument="${RTS_BATCH_RHI:--RenderOffscreen}"
LowCostCVars="${RTS_BATCH_CVARS-sg.ShadowQuality=0,sg.GlobalIlluminationQuality=0,sg.ReflectionQuality=0,sg.PostProcessQuality=0,sg.EffectsQuality=0,sg.FoliageQuality=0,sg.ViewDistanceQuality=0,r.ScreenPercentage=25}"
ScriptDirectory="$(cd "$(dirname "$0")" && pwd)"
Summary="$LogDirectory/summary.txt"
mkdir -p "$LogDirectory"

# Merges the low scalability cvars into the scenario's own -DPCVars (the engine reads only the first one).
BuildArguments()
{
	local Arguments="$1" Merged="" Argument HasCVars=0
	for Argument in $Arguments; do
		case "$Argument" in
			-DPCVars=*)
				HasCVars=1
				[ -n "$LowCostCVars" ] && Argument="$Argument,$LowCostCVars"
				;;
		esac
		Merged="$Merged $Argument"
	done
	if [ "$HasCVars" -eq 0 ] && [ -n "$LowCostCVars" ]; then
		Merged="$Merged -DPCVars=$LowCostCVars"
	fi
	echo "$Merged"
}

RunOne()
{
	local Name="$1" TimeoutSeconds="$2" Arguments
	Arguments="$(BuildArguments "$3")"
	local GameLog="$LogDirectory/game_$Name.log" DebugLog="$LogDirectory/dbg_$Name.txt"
	# shellcheck disable=SC2086
	bash "$ScriptDirectory/run_under_lldb.sh" "$Executable" "$DebugLog" "$TimeoutSeconds" $Arguments \
		-unattended -nosound "$RhiArgument" -windowed "-ResX=$ResX" "-ResY=$ResY" \
		"-ABSLOG=$(cygpath -w "$GameLog")" > /dev/null 2>&1
	local Breaks Exceptions Ending Results
	Breaks=$(grep -c "RTS_DBG BREAK" "$DebugLog" 2>/dev/null)
	Exceptions=$(grep -c "RTS_DBG EXCEPTION second_chance" "$DebugLog" 2>/dev/null)
	Ending=$(grep -oE "RTS_DBG (exited code=-?[0-9]+|timeout after [0-9]+s|CRASH)" "$DebugLog" 2>/dev/null | tail -1)
	Results=$(grep -aoE "RTS_[A-Z0-9_]+ RESULT [^\r]*|RTS_NOMADIC_BXP_TEST_RESULT [^\r]*|RTS_UNIT_ABILITY_STRESS_RESULT [^\r]*" "$GameLog" 2>/dev/null | cut -c1-200 | paste -sd ';' -)
	echo "$Name | breaks=$Breaks crash=$Exceptions | $Ending | $Results" >> "$Summary"
}

Lane()
{
	local LaneIndex="$1" LineIndex=0 Name TimeoutSeconds Arguments
	while IFS='|' read -r Name TimeoutSeconds Arguments; do
		[ -z "$Name" ] && continue
		if [ $((LineIndex % Lanes)) -eq "$LaneIndex" ]; then
			RunOne "$Name" "$TimeoutSeconds" "$Arguments"
		fi
		LineIndex=$((LineIndex + 1))
	done < <(grep -v '^\s*#' "$BatchFile" | grep -v '^\s*$')
}

echo "batch started $(date +%H:%M:%S) lanes=$Lanes rhi=$RhiArgument res=${ResX}x$ResY exe=$Executable" >> "$Summary"
for LaneIndex in $(seq 0 $((Lanes - 1))); do
	Lane "$LaneIndex" &
done
wait
echo "batch finished $(date +%H:%M:%S)" >> "$Summary"
cat "$Summary"
