#!/bin/sh
# Mission end screen check: ends the mission on RTS_ToCopySmall headless (Development editor build, offscreen) and
# copies the debrief screenshots to Saved/MissionDebrief/<Victory|Defeat>/. Never touches the shared editor.
# usage: sh run_debrief_check.sh [victory|defeat] [WxH] [extra ExecCmds, e.g. "RTS.Debrief.LightScale 1.2"]
# Grep: MISSION_DEBRIEF (stage running / buttons revealed / PASS|FAIL).
export MSYS_NO_PATHCONV=1
ROOT=D:/UE5Projects/RTS_Survival
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
OUTCOME=${1:-victory}
RES=${2:-1280x720}
EXTRA=${3:-}
W=${RES%x*}; H=${RES#*x}
SWITCH=""
[ "$OUTCOME" = "defeat" ] && SWITCH="-MissionDebriefDefeat"
LOG=$ROOT/Saved/Logs/MissionDebrief_$OUTCOME.log
rm -f "$ROOT"/Saved/MissionDebrief/Victory/*.png "$ROOT"/Saved/MissionDebrief/Defeat/*.png
CMDS="r.TextureStreaming 0, r.SetRes ${W}x${H}w"
[ -n "$EXTRA" ] && CMDS="$CMDS, $EXTRA"
"$ENGINE" $ROOT/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nosound \
  -RenderOffscreen -windowed -ResX=$W -ResY=$H -DisablePlugins=Fab,BpGeneratorUltimate \
  -ExecCmds="$CMDS" -ForceRes -MissionDebriefValidate $SWITCH -abslog=$LOG > /dev/null 2>&1
grep -E "MISSION_DEBRIEF (stage|buttons|FAIL|done)" $LOG | cut -c1-200
