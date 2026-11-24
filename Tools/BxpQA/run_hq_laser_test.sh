#!/bin/sh
# Headless -HqLaserBxpValidate (Buildings/BuildingExpansion/Tests/HqLaserBxpTestSubsystem) on the Development editor:
# HQ deploy, laser option click + socket placement, price, type, weapon, holes, fire at an enemy T-34.
LOG="D:/UE5Projects/RTS_Survival/Saved/BxpQA/hq_laser_test_$(date +%H%M%S).log"
export MSYS_NO_PATHCONV=1
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game \
  -unattended -nosound -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding \
  -HqLaserBxpValidate -HqLaserAnchorX=-2642 -HqLaserAnchorY=-4082 -abslog=$LOG > /dev/null 2>&1
echo "LOG $LOG"
grep -E "RTS_HQLASER_TEST (CHECK|RESULT|START)" "$LOG"
