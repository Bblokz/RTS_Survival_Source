#!/usr/bin/env bash
# Pictures the aim ability reticles (UAimCrosshairCaptureSubsystem) in an offscreen game run: no window, the open
# editor is not touched. Output: <output folder>/<Case>_spot<N>_<Game|Close|Far>.png; contact_sheet.py lays them out.
# Usage: run_capture.sh [output folder] [extra UE switches...]
#   WIRE=44 run_capture.sh ...    gives the spawned aim actor the new crosshair materials and UnitsPerScale 44,
#                                 for checking them before BP_PlayerAimAbilityCursor itself is wired.
#   AT="X,Y;X,Y" run_capture.sh   pictures every case at these ground points instead of the map's anchor.
#   CASES="Label=AimType:Radius[:/Game/Material]+..." picks the reticles (default: all six aim types).
#   EXE=Development run_capture.sh   uses UnrealEditor-Cmd.exe instead of the DebugGame one.
# Unreal reads the first copy of a switch, so pass changes through the variables above.
# While a run is going the editor cannot save the packages it has loaded (the map, the materials).
set -u
OUT="${1:-D:/UE5Projects/RTS_Survival/Saved/AimCrosshairs}"
shift || true
MAP="${MAP:-/Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_GameStartTutorial}"
SKY_FILL="${SKY_FILL:-3}"
EXPOSURE_BIAS="${EXPOSURE_BIAS:-1.0}"
ENGINE_DIR="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64"
if [ "${EXE:-DebugGame}" = "Development" ]; then
  ENGINE="$ENGINE_DIR/UnrealEditor-Cmd.exe"
else
  ENGINE="$ENGINE_DIR/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
EXTRA=()
if [ -n "${WIRE:-}" ]; then
  EXTRA+=(-AimCrosshairWireUnitsPerScale="$WIRE")
fi
if [ -n "${AT:-}" ]; then
  EXTRA+=(-AimCrosshairAt="$AT")
fi
if [ -n "${CASES:-}" ]; then
  EXTRA+=(-AimCrosshairCases="$CASES")
fi
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/AimCrosshairs.log"
mkdir -p "$OUT"
rm -f "$OUT"/*.png
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1920 -ResY=1080 -DisablePlugins=Fab \
  -benchmark -fps=30 \
  -ExecCmds="r.TextureStreaming 0" \
  -AimCrosshairCapture -AimCrosshairSkyFill="$SKY_FILL" -AimCrosshairExposureBias="$EXPOSURE_BIAS" \
  -AimCrosshairDir="$OUT" -abslog="$LOG" "${EXTRA[@]}" "$@"
echo "exit=$? log=$LOG"
grep "RTS_AIM_CROSSHAIR" "$LOG" | tail -60
grep -iE "M_AimCrosshair.*(error|failed)" "$LOG" | head -20
