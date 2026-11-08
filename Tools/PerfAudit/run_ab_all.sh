#!/usr/bin/env bash
# A/B engine-setting variants on the three audit maps at 2560x1080 (variants in Saved/PerfAudit/ab_variants.txt).
# Usage: run_ab_all.sh <suffix> [maps: fm marder desert]
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/PerfAudit"
SUF="$1"; shift
MAPS="${*:-fm marder desert}"
VAR=$(cat D:/UE5Projects/RTS_Survival/Saved/PerfAudit/ab_variants.txt)
for m in $MAPS; do
  case $m in
    fm) MAP=/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission; V="-32118,32400,2952,-70,0;-4074,-21000,2950,-70,0;-28074,-21000,2950,-70,0;-28074,9000,2950,-70,0";;
    marder) MAP=/Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission; V="-4074,-21000,2689,-70,0;-4074,3000,3928,-70,0";;
    desert) MAP=/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01; V="-22074,-33000,3485,-70,0;7926,-33000,3433,-70,0;-10074,-9000,4285,-70,0;14926,-3525,3322,-70,0";;
  esac
  RES=2560x1080 EXEC="ShowFlag.PostProcessMaterial 0" "$HERE/run_gpu_probe.sh" "${m}_ab_$SUF" "$MAP" "-PerfProbeViews=$V" \
    "-PerfProbeVariants=$VAR" -PerfProbeRepeats=2 -PerfProbeWarmup=120 -PerfProbeFrames=200 -PerfProbeProfileGPU | grep -E "summary|RESULT" | sed 's/cmds=.*//'
done
