#!/usr/bin/env bash
# Ice-break test (UFrozenLakeIceTestSubsystem, -FrozenLakeIceTest) on Ger_FrozenLake in an offscreen game run:
# a heavy tank on grey ice must break through and sink; a medium on grey ice and a heavy on heavy ice must not.
# Usage: run_ice_break_test.sh <run name> [extra UE switches...]
#   BIN=DebugGame run_ice_break_test.sh ...   default Development (UnrealEditor-Cmd.exe)
# Output: Saved/FrozenLakeIceTest/<run name>/ (pictures), log Saved/Logs/FrozenLakeIceTest_<run name>.log.
set -u
NAME="$1"; shift
MAP="${MAP:-/Game/RTS_Survival/Maps/GerMissions/Ger_FrozenLake}"
if [ "${BIN:-Development}" = "DebugGame" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
fi
OUT="D:/UE5Projects/RTS_Survival/Saved/FrozenLakeIceTest/$NAME"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/FrozenLakeIceTest_$NAME.log"
rm -rf "$OUT"
mkdir -p "$OUT"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1920 -ResY=1080 -DisablePlugins=Fab \
  -FrozenLakeIceTest -FrozenLakeIceTestDir="$OUT" -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "RTS_FL_ICE_TEST|Frozen lake ice" "$LOG" | sed 's/.*\(RTS_FL_ICE_TEST\|Frozen lake ice\)/\1/'
