#!/usr/bin/env bash
# Opens and closes the two Ore Mountains takeovers (capturing trucks field manual, convoy manifest document) in an
# offscreen DebugGame game run: UOreTakeoversValidateSubsystem, -OreTakeoversValidate.
# Usage: run_validate.sh [extra UE switches]
#   MAP=/Game/... run_validate.sh       map to run on (default RTS_ToCopySmall)
# Output: Saved/InfoTakeover/OreTakeovers/<height>p/*.png, log Saved/Logs/OreTakeoversValidate.log.
set -u
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/OreTakeoversValidate.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -OreTakeoversValidate -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "ORE_TAKEOVERS" "$LOG" | sed "s/.*ORE_TAKEOVERS /ORE_TAKEOVERS /"
