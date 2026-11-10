#!/usr/bin/env bash
# Validates add-on armour panels on a real tank (UAddOnArmorPanelsValidateSubsystem) in an offscreen game run: no
# window, the open editor is not touched. Pictures land in Saved/AddOnArmorBreak/<tag>/, results in the log
# (grep RTS_ADDON_PANELS; the last line is RESULT PASS|FAIL).
# Usage: run_panels_validate.sh [extra UE switches...]
#   VEHICLE=<class path>     tank Blueprint class (default: Panzer II F spaced armour)
#   INJECT=<hull mesh>       add the panels component and this hull to a Blueprint that has neither yet
#   INJECT_REPLACE=<mesh object path>  the mesh INJECT replaces (default: the Hull component's), e.g. a bolted-on mesh
#   TAG=name MAP=/Game/...   picture folder and map (default RTS_ToCopySmall: flat and quiet, but the Brummbar falls
#                            through the ground at its spawn; UT_B4Howitzer works, its artillery can hit slow runs)
#   PROJECT=<uproject> ENGINE=<editor exe>   e.g. the DebugGame editor when the Development DLL is locked
#   -AddOnArmorPanelsTraceDebris        logs the first hit's debris pieces every frame (debris checks always run)
#   -AddOnArmorPanelsRevealTank         takes the test tank out of the fog of war, so the debris checks also run on
#                                       enemy-faction (e.g. Soviet) tanks, which the fog hides otherwise
#   -AddOnArmorPanelsFirstHitPanel=<i>  the first shell hits left panel <i> (layout index) instead of the middle one and
#                                       checks its burst throws that panel's debris mesh, e.g. an end plate's own mesh
#   -AddOnArmorPanelsStartModuleSetup   the test starts the vehicle module setup on Blueprints that never do
#                                       (RTS_BP_AssaultTank children); logged as a NOTE
set -u
PROJECT="${PROJECT:-D:/UE5Projects/RTS_Survival/RTS_Survival.uproject}"
ENGINE="${ENGINE:-D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe}"
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
TAG="${TAG:-PanelsValidate}"
OUT="D:/UE5Projects/RTS_Survival/Saved/AddOnArmorBreak"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/AddOnArmorPanels_${TAG}.log"
EXTRA=()
if [ -n "${VEHICLE:-}" ]; then
  EXTRA+=("-AddOnArmorPanelsVehicle=${VEHICLE}")
fi
if [ -n "${INJECT:-}" ]; then
  EXTRA+=("-AddOnArmorPanelsInject=${INJECT}")
fi
if [ -n "${INJECT_REPLACE:-}" ]; then
  EXTRA+=("-AddOnArmorPanelsInjectReplace=${INJECT_REPLACE}")
fi
MSYS_NO_PATHCONV=1 "$ENGINE" "$PROJECT" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab,BpGeneratorUltimate \
  -benchmark -fps=30 -ExecCmds="r.TextureStreaming 0" \
  -AddOnArmorPanelsValidate -AddOnArmorPanelsTag="$TAG" -AddOnArmorPanelsDir="$OUT" \
  -abslog="$LOG" "${EXTRA[@]}" "$@"
echo "exit=$? log=$LOG"
grep "RTS_ADDON_PANELS\|AddOnArmorPanels" "$LOG" | tail -60
