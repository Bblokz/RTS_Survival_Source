#!/usr/bin/env bash
# Takes raw unit portrait pictures in an offscreen game run (no window; the open editor is not touched).
# Usage: run_capture.sh <unit list file> [output folder] [extra UE switches...]
#   SKY_FILL=3 EXPOSURE_BIAS=0.3 run_capture.sh ...   overrides the lighting defaults.
# -benchmark -fps=30 steps game time by a fixed 1/30 s per frame, so the pictures after a shot land on the same
# frames however slow the PNG export makes a frame.
# The sky fill lifts the shadows: scene captures get no sky light from Lumen on this map, so without it every
# shadow is black. Unreal reads the first copy of a switch, so pass changes through the variables above.
# Needs the Development editor target built (the user's editor runs DebugGame, so there is no DLL lock).
set -u
LIST="$(cygpath -w "$(realpath "$1")")"
OUT="${2:-D:/UE5Projects/RTS_Survival/Saved/UnitPortraits}"
shift; shift || true
SKY_FILL="${SKY_FILL:-3}"
EXPOSURE_BIAS="${EXPOSURE_BIAS:-0.3}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/UnitPortraits.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" \
  /Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_GameStartTutorial \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -benchmark -fps=30 \
  -ExecCmds="r.TextureStreaming 0" \
  -UnitPortraitSkyFill="$SKY_FILL" -UnitPortraitExposureBias="$EXPOSURE_BIAS" \
  -UnitPortraitCapture="$LIST" -UnitPortraitDir="$OUT" -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep "RTS_PORTRAIT" "$LOG" | tail -60
