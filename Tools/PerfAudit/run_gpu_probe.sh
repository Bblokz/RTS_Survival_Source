#!/usr/bin/env bash
# GPU probe of a map (UPerfProbeSubsystem, -PerfProbe) in an offscreen -game run at Epic scalability.
# Usage: run_gpu_probe.sh <name> <map package> [extra UE switches]
#   -PerfProbeViews="X,Y,Z,Pitch,Yaw;..."   fixed cameras (default: the player camera at game start)
#   -PerfProbeVariants="cmd&cmd|cmd"         A/B console variants measured alternately per view
#   -PerfProbeRepeats=N -PerfProbeFrames=N -PerfProbeWarmup=N -PerfProbeProfileGPU -PerfProbeKeepUI
#   RES=1920x1080 (default)   EXEC="r.Foo 0"  extra console commands at start (comma separated)
#   BIN=Development (default) | DebugGame
# Output: log Saved/Logs/PerfProbe_<name>.log, shots Saved/PerfAudit/Shots/<name>_view<N>.png
set -u
NAME="$1"; shift
MAP="$1"; shift
RES="${RES:-1920x1080}"
EXEC_CMDS="r.SetRes ${RES}w, r.VSync 0, t.MaxFPS 0, r.ProfileGPU.ShowUI 0, scalability 3, r.ShowMaterialDrawEvents 1, r.EmitMeshDrawEvents 1"
if [ -n "${EXEC:-}" ]; then EXEC_CMDS="$EXEC_CMDS, $EXEC"; fi
if [ "${BIN:-Development}" = "DebugGame" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
fi
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/PerfProbe_$NAME.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX="${RES%x*}" -ResY="${RES#*x}" -NoLoadingScreen \
  -DisablePlugins=Fab,BpGeneratorUltimate -ExecCmds="$EXEC_CMDS" -PerfProbe -PerfProbeName="$NAME" -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "RTS_PERF_PROBE" "$LOG" | sed 's/.*RTS_PERF_PROBE //'
