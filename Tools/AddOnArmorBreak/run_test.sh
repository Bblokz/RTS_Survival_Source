#!/usr/bin/env bash
# Runs the add-on armour break comparison (UAddOnArmorBreakTestSubsystem) in an offscreen game run: no window, the
# open editor is not touched. Results land in Saved/AddOnArmorBreak/<tag>/ (pictures) and <tag>_frames.csv (perf).
# Usage: run_test.sh <Spike|Capture|Perf> [extra UE switches...]
#   TECHNIQUE=Niagara|GeometryCollection|None   technique for Spike and Perf (Capture always shows both)
#   VEHICLES=25 HITS=10 TAG=name SEED=1         perf settings
#   MAP=/Game/...                               map (default: UT_B4Howitzer for pictures, RTS_ToCopySmall for perf)
#   PROJECT=<uproject> ENGINE=<editor exe>      e.g. an isolated copy when the shared DLLs are locked
#   TRACE=1                                     also writes an Unreal trace for Insights (costs ~20% frame time)
set -u
MODE="${1:-Spike}"
shift || true
PROJECT="${PROJECT:-D:/UE5Projects/RTS_Survival/RTS_Survival.uproject}"
ENGINE="${ENGINE:-D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe}"
TECHNIQUE="${TECHNIQUE:-Niagara}"
VEHICLES="${VEHICLES:-25}"
HITS="${HITS:-10}"
SEED="${SEED:-1}"
OUT="D:/UE5Projects/RTS_Survival/Saved/AddOnArmorBreak"
if [ "$MODE" = "Perf" ]; then
  MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
  RES=(-ResX=1920 -ResY=1080)
else
  MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/UT_B4Howitzer}"
  RES=(-ResX=1280 -ResY=720)
fi
TAG="${TAG:-${MODE}_${TECHNIQUE}_v${VEHICLES}_h${HITS}}"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/AddOnArmorBreak_${TAG}.log"
EXTRA=()
if [ "${TRACE:-0}" = "1" ]; then
  EXTRA=(-trace=cpu,frame,bookmark -statnamedevents "-tracefile=${OUT}/${TAG}.utrace")
fi
MSYS_NO_PATHCONV=1 "$ENGINE" "$PROJECT" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed "${RES[@]}" -DisablePlugins=Fab,BpGeneratorUltimate \
  -benchmark -fps=30 -ExecCmds="r.TextureStreaming 0" \
  -AddOnArmorBreakTest="$MODE" -AddOnArmorBreakTechnique="$TECHNIQUE" -AddOnArmorBreakVehicles="$VEHICLES" \
  -AddOnArmorBreakHitsPerSecond="$HITS" -AddOnArmorBreakSeed="$SEED" -AddOnArmorBreakTag="$TAG" \
  -AddOnArmorBreakDir="$OUT" -AddOnArmorBreakSkyFill=3 -AddOnArmorBreakExposureBias=1.0 \
  -abslog="$LOG" "${EXTRA[@]}" "$@"
echo "exit=$? log=$LOG"
grep "RTS_ADDON_ARMOR" "$LOG" | grep -v "spike piece\|spike burst" | tail -40
