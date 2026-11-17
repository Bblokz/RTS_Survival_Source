#!/bin/sh
# Headless -SDBxpRoundTwoValidate (Buildings/BuildingExpansion/Tests/SDBxpRoundTwoTestSubsystem) on the Development
# editor build. usage: run_round2_test.sh [anchorX anchorY] ; log in Saved/SDBxpRound2/test_<time>.log
AX="${1:--2642}"; AY="${2:--4082}"
LOG="D:/UE5Projects/RTS_Survival/Saved/SDBxpRound2/test_$(date +%H%M%S).log"
export MSYS_NO_PATHCONV=1
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game \
  -unattended -nosound -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding \
  -SDBxpRoundTwoValidate -SDBxpAnchorX=$AX -SDBxpAnchorY=$AY \
  -LogCmds="LogWallHoleDamage Verbose, LogPanzerwerferReloadBonus Log" -abslog=$LOG > /dev/null 2>&1
echo "LOG $LOG"
grep -E "RTS_SDBXP2_TEST|RTS_PZW_RELOAD_BONUS" "$LOG"
