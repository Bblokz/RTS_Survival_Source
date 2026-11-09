#!/usr/bin/env bash
# Runs the shell crater benchmark (UImpactDecalBenchmarkSubsystem, -ImpactDecalBench) in an offscreen game run:
# 100 craters over 10 s, held, 100 more recycling the oldest, 100 in one frame; pooled (ULandscapeImpactDecalSubsystem)
# against legacy (a new decal component per crater, like UGameDecalManager). See Docs/Environment/ImpactDecals.md.
# Usage: run_bench.sh <run name> [extra UE switches...]
#   run_bench.sh base                                   2 repeats of every step, both paths
#   run_bench.sh gpu -ImpactDecalBenchProfileGPU         also one ProfileGPU dump per Hold step
#   MATERIAL=/Game/...MI_X.MI_X run_bench.sh standin     crater material override (default: the settings material)
#   BIN=Development run_bench.sh ...                     default DebugGame (the shared editor holds Development)
#   MAP=/Game/... run_bench.sh ...                       default Ger_Trains; -ImpactDecalBenchAt=X,Y for the spot
#   run_bench.sh live -ImpactDecalBenchLive              map's own fighting for 90 s (craters only on landscape?) and
#                                                        the Shell Craters menu combo through Apply (Off, 200, restore)
# The editor's GameUserSettings.ini is backed up and restored around the run (the menu check's Apply saves it).
# Output: Saved/ImpactDecalBench/<run name>/ (frame CSV + picture), log Saved/Logs/ImpactDecalBench_<run name>.log.
set -u
NAME="$1"; shift
MAP="${MAP:-/Game/RTS_Survival/Maps/GerMissions/Ger_Trains}"
RES="${RES:-1920x1080}"
EXEC_CMDS="r.VSync 0, t.MaxFPS 0, r.ProfileGPU.ShowUI 0, r.SetRes ${RES}w"
if [ -n "${EXEC:-}" ]; then EXEC_CMDS="$EXEC_CMDS, $EXEC"; fi
if [ "${BIN:-DebugGame}" = "Development" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
EXTRA=()
if [ -n "${MATERIAL:-}" ]; then
  EXTRA+=("-ini:Game:[/Script/RTS_Survival.RTSImpactDecalSettings]:DecalMaterial=${MATERIAL}")
fi
OUT="D:/UE5Projects/RTS_Survival/Saved/ImpactDecalBench/$NAME"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/ImpactDecalBench_$NAME.log"
rm -rf "$OUT"
mkdir -p "$OUT"
USER_SETTINGS="D:/UE5Projects/RTS_Survival/Saved/Config/WindowsEditor/GameUserSettings.ini"
[ -f "$USER_SETTINGS" ] && cp "$USER_SETTINGS" "$OUT/GameUserSettings.backup.ini"
# -benchmark -fps=30: fixed game step, so 10 s of craters is always 300 frames; wall time per frame is measured.
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX="${RES%x*}" -ResY="${RES#*x}" \
  -DisablePlugins=Fab,BpGeneratorUltimate -benchmark -fps=30 -ExecCmds="$EXEC_CMDS" \
  -ImpactDecalBench -ImpactDecalBenchDir="$OUT" -abslog="$LOG" "${EXTRA[@]}" "$@"
EXIT=$?
[ -f "$OUT/GameUserSettings.backup.ini" ] && cp "$OUT/GameUserSettings.backup.ini" "$USER_SETTINGS"
echo "exit=$EXIT log=$LOG"
grep -E "RTS_IMPACT_DECAL(_BENCH|S) " "$LOG" | sed 's/.*RTS_IMPACT_DECAL_BENCH //'
