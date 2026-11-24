#!/usr/bin/env bash
# Runs the small arms fight bench (FTraceImpactVfxPerfScenario) headless-ish in a throwaway DebugGame game window and
# prints the RTS_MUZZLE_VFX / RTS_TRACE_IMPACT_VFX lines. Docs/VFX/SmallArmsMuzzleVfx.md.
#   run_fight_bench.sh <label> [extra args]
#   extra: -SmallArmsMuzzleVfxLegacy (old per-weapon effects), -TraceImpactVfxScreenshots (pictures in Saved/Screenshots),
#          -TraceImpactVfxSquads=N -TraceImpactVfxFrames=N -TraceImpactVfxTanks=N
set -u
LABEL="${1:-run}"; shift || true
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/MuzzleVfx_Bench_${LABEL}.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" -game -windowed -resx=1600 -resy=900 \
  -TraceImpactVfxMeasure -TraceImpactVfxTanks=0 "$@" -unattended -nosplash -NoLiveCoding \
  -DisablePlugins=Fab,BpGeneratorUltimate -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_MUZZLE_VFX|RTS_TRACE_IMPACT_VFX (phase|RESULT|screenshot|scheduled)" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | sort -u | head -60
