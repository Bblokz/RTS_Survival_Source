#!/bin/sh
# Headless -BxpHolesQAValidate (Buildings/BuildingExpansion/Tests/BxpHolesQATestSubsystem) on the Development editor.
# usage: run_holes_test.sh [only-substring]; log in Saved/BxpQA/holes_test_<time>.log
ONLY="${1:+-BxpQAOnly=$1}"
LOG="D:/UE5Projects/RTS_Survival/Saved/BxpQA/holes_test_$(date +%H%M%S).log"
export MSYS_NO_PATHCONV=1
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game \
  -unattended -nosound -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding \
  -BxpHolesQAValidate -BxpQAAnchorX=-2642 -BxpQAAnchorY=-4082 $ONLY -abslog=$LOG > /dev/null 2>&1
echo "LOG $LOG"
grep -E "RTS_BXPQA_TEST (CHECK FAIL|RESULT|START)" "$LOG"
grep -cE "RTS_BXPQA_TEST CHECK PASS" "$LOG"
