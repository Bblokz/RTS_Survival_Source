#!/bin/sh
# Headless -BreakthroughBunkersValidate (Buildings/BuildingExpansion/Tests/BreakthroughBunkersTestSubsystem) on the
# Development editor: Breakthrough HQ Barriers tab, then the Flak 88, Flakvierling and Pz 38(t) bunkers built by its queue.
LOG="D:/UE5Projects/RTS_Survival/Saved/BxpQA/bt_bunkers_test_$(date +%H%M%S).log"
export MSYS_NO_PATHCONV=1
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game \
  -unattended -nosound -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding \
  -BreakthroughBunkersValidate -BtBunkersAnchorX=-2642 -BtBunkersAnchorY=-4082 -abslog=$LOG > /dev/null 2>&1
echo "LOG $LOG"
grep -E "RTS_BTBUNKERS_TEST (CHECK|RESULT|START)" "$LOG"
