#!/usr/bin/env bash
# Pictures impact Niagara systems in an offscreen game run (no window; the open editor is not touched).
# Usage: run_capture.sh <system path>[+<system path>...] [output folder] [extra UE switches...]
#   SCALE=1.5 run_capture.sh ...          plays the systems at the weapon ImpactScale they will get.
#   MAP=/Game/... AT=X,Y run_capture.sh   picks another map / impact point (see UImpactVfxCaptureSubsystem).
# The pictures land in <output folder>/<system name>/; contact_sheet.py lays them out per system.
# While a run is going the editor cannot save the packages it has loaded (the map, the systems): keep runs short
# and do not run on a map the user is editing.
# Needs the Development editor target built (the user's editor runs DebugGame, so there is no DLL lock).
set -u
SYSTEMS="$1"
OUT="${2:-D:/UE5Projects/RTS_Survival/Saved/ImpactVfx}"
shift; shift || true
SCALE="${SCALE:-1.0}"
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/UT_B4Howitzer}"
SKY_FILL="${SKY_FILL:-3}"
EXPOSURE_BIAS="${EXPOSURE_BIAS:-1.0}"
LOCATION_SWITCH=()
if [ -n "${AT:-}" ]; then
  LOCATION_SWITCH=(-ImpactVfxAt="$AT")
fi
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/ImpactVfx.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -benchmark -fps=30 \
  -ExecCmds="r.TextureStreaming 0" \
  -ImpactVfxSkyFill="$SKY_FILL" -ImpactVfxExposureBias="$EXPOSURE_BIAS" -ImpactVfxScale="$SCALE" \
  "${LOCATION_SWITCH[@]}" -ImpactVfxCapture="$SYSTEMS" -ImpactVfxDir="$OUT" -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep "RTS_IMPACT_VFX" "$LOG" | tail -60
