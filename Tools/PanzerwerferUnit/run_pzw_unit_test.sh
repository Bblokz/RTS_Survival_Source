#!/bin/sh
# Headless -PanzerwerferUnitValidate (Units/Tanks/Tests/PanzerwerferUnit) on RTS_ToCopySmall.
# usage: run_pzw_unit_test.sh [DebugGame|Development] [anchorX anchorY] ; log in Saved/PanzerwerferUnit/test_<time>.log
CONFIG="${1:-DebugGame}"; AX="${2:--2642}"; AY="${3:--4082}"
BIN="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64"
EXE="$BIN/UnrealEditor-Win64-DebugGame-Cmd.exe"
[ "$CONFIG" = "Development" ] && EXE="$BIN/UnrealEditor-Cmd.exe"
mkdir -p D:/UE5Projects/RTS_Survival/Saved/PanzerwerferUnit
LOG="D:/UE5Projects/RTS_Survival/Saved/PanzerwerferUnit/test_$(date +%H%M%S).log"
export MSYS_NO_PATHCONV=1
"$EXE" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game \
  -unattended -nosound -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding \
  -PanzerwerferUnitValidate -PzwAnchorX=$AX -PzwAnchorY=$AY \
  -LogCmds="LogPanzerwerferReloadBonus Log" -abslog=$LOG > /dev/null 2>&1
echo "LOG $LOG"
grep -E "RTS_PZW_UNIT_TEST|RTS_PZW_RELOAD_BONUS" "$LOG"
