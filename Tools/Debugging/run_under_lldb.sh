#!/bin/bash
# Runs a packaged RTS_Survival executable (the child exe under <Package>/Windows/RTS_Survival/Binaries/Win64, not the
# launcher, which would start the game as a separate process) under Rider's bundled lldb with rts_debug_driver.py.
# The driver logs every ensure break and exception with a symbolized stack, steps over ensure breaks, dumps all
# threads and writes a minidump on a crash, and kills the game after the time limit.
#
# Usage: run_under_lldb.sh <child exe> <debug log> <max seconds> [game arguments...]
# Example:
#   run_under_lldb.sh D:/RR_test/ShippingMapTests_DebugGame/Windows/RTS_Survival/Binaries/Win64/RTS_SurvivalShippingMapTests-Win64-DebugGame.exe \
#     D:/RR_test/debug_run.txt 3600 -RTSRunMapTest=All -RTSTestExitOnComplete -unattended -RenderOffscreen -windowed -ResX=1280 -ResY=720 -log
set -u
Executable="$1"
DebugLog="$2"
MaxSeconds="$3"
shift 3
Lldb="${RTS_LLDB:-/c/Users/$USERNAME/AppData/Local/Programs/Rider/plugins/cidr-debugger/bin/lldb/win/x64/bin/lldb.exe}"
ScriptDirectory="$(cd "$(dirname "$0")" && pwd)"
JoinedArguments=""
for Argument in "$@"; do
	JoinedArguments="${JoinedArguments}${Argument}|"
done
export RTS_DBG_EXE="$(cygpath -w "$Executable")"
export RTS_DBG_CWD="$(cygpath -w "$(dirname "$Executable")")"
export RTS_DBG_LOG="$(cygpath -w "$DebugLog")"
export RTS_DBG_DUMP="$(cygpath -w "${DebugLog%.*}.dmp")"
export RTS_DBG_MAX_SECONDS="$MaxSeconds"
export RTS_DBG_ARGS="$JoinedArguments"
MSYS_NO_PATHCONV=1 "$Lldb" -b -o "command script import $(cygpath -w "$ScriptDirectory/rts_debug_driver.py")"
grep -E "RTS_DBG (launched|BREAK|EXCEPTION|CRASH|exited|timeout|save-core)" "$DebugLog"
