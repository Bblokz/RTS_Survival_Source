#!/usr/bin/env bash
# Runs the grenade benchmark (UGrenadeBenchmarkSubsystem, -GrenadeBenchmark) in an offscreen game run.
# Usage: run_bench.sh <run name> [extra UE switches...]
#   EXEC="RTS.Grenade.Legacy 1" run_bench.sh baseline        extra console commands (comma separated)
#   run_bench.sh tune1 -GrenadeBenchOnly=AT_ -GrenadeBenchOverride="GerAtGrenade:AoeRange=200"
#   BIN=Development run_bench.sh ...                          default DebugGame (the user's editor holds Development)
# Output: Saved/GrenadeBench/<run name>/ (CSV + pictures), log Saved/Logs/GrenadeBench_<run name>.log.
set -u
NAME="$1"; shift
EXEC_EXTRA="${EXEC:-}"
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
if [ "${BIN:-DebugGame}" = "Development" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
OUT="D:/UE5Projects/RTS_Survival/Saved/GrenadeBench/$NAME"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/GrenadeBench_$NAME.log"
rm -rf "$OUT"
EXEC_CMDS="r.TextureStreaming 0"
if [ -n "$EXEC_EXTRA" ]; then EXEC_CMDS="$EXEC_CMDS, $EXEC_EXTRA"; fi
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -benchmark -fps=30 -ExecCmds="$EXEC_CMDS" -GrenadeBenchmark -GrenadeBenchDir="$OUT" -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "RTS_GRENADE_BENCH (summary|check=FAIL|RESULT)" "$LOG" | sed 's/.*RTS_GRENADE_BENCH //'
