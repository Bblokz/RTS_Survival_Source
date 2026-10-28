#!/usr/bin/env bash
# Runs the tank trail benchmark (UTankTrailBenchmarkSubsystem, -TankTrailBench) in an offscreen game run on Ger_Trains:
# tanks shuttle over the grass while phases with rts.TankTrails 0 and 1 alternate (Docs/Environment/TankTrails.md).
# Usage: run_bench.sh <run name> [extra UE switches...]
#   run_bench.sh base                                  40 tanks, 10 off/on pairs of 240 frames
#   run_bench.sh gpu -TankTrailBenchProfileGPU          also one ProfileGPU pass dump per phase in the log
#   run_bench.sh many -TankTrailBenchCount=80          more tanks
#   BIN=Development run_bench.sh ...                   default DebugGame (the user's editor holds Development)
#   RES=2560x1440 run_bench.sh ...                     render resolution, default 1920x1080
#   EXEC="grass.Enable 0" run_bench.sh nograss ...     extra console commands at start (comma separated)
# Output: Saved/TankTrailBench/<run name>/ (frame CSV + pictures), log Saved/Logs/TankTrailBench_<run name>.log.
set -u
NAME="$1"; shift
MAP="${MAP:-/Game/RTS_Survival/Maps/GerMissions/Ger_Trains}"
RES="${RES:-1920x1080}"
EXEC_CMDS="r.VSync 0, t.MaxFPS 0, r.ProfileGPU.ShowUI 0"
if [ -n "${EXEC:-}" ]; then EXEC_CMDS="$EXEC_CMDS, $EXEC"; fi
if [ "${BIN:-DebugGame}" = "Development" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
OUT="D:/UE5Projects/RTS_Survival/Saved/TankTrailBench/$NAME"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/TankTrailBench_$NAME.log"
rm -rf "$OUT"
mkdir -p "$OUT"
# -benchmark -fps=30: fixed game step, so every phase simulates the same driving; wall time per frame is measured.
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX="${RES%x*}" -ResY="${RES#*x}" -DisablePlugins=Fab \
  -benchmark -fps=30 -ExecCmds="$EXEC_CMDS" -TankTrailBench -TankTrailBenchDir="$OUT" -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "RTS_TANK_TRAIL_BENCH (started|navigation|spawned|phase|pairdelta|summary|pictures|RESULT)" "$LOG" | sed 's/.*RTS_TANK_TRAIL_BENCH //'
