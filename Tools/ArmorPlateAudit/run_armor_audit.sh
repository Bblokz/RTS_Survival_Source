#!/usr/bin/env bash
# Audits vehicle armour boxes (UArmorPlateAuditSubsystem) in an offscreen game run: no window, the open editor is not
# touched. Each listed class spawns in turn; the log shows its modules, every plate box in actor space and the plate
# that shells from the left, right, front and rear resolve (grep RTS_ARMOR_AUDIT; the last line is RESULT).
# Usage: run_armor_audit.sh [extra UE switches...]
#   LIST=<file>              one Blueprint class path per line (default: assault_tanks.txt next to this script)
#   TAG=name MAP=/Game/...   log name and map (default UT_B4Howitzer; RTS_ToCopySmall drops the Brummbar through
#                            the ground at the spawn)
#   PROJECT=<uproject> ENGINE=<editor exe>   e.g. the DebugGame editor when the Development DLL is locked
set -u
HERE="$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)"
PROJECT="${PROJECT:-D:/UE5Projects/RTS_Survival/RTS_Survival.uproject}"
ENGINE="${ENGINE:-D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe}"
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/UT_B4Howitzer}"
LIST="${LIST:-$HERE/assault_tanks.txt}"
TAG="${TAG:-ArmorPlateAudit}"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/ArmorPlateAudit_${TAG}.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "$PROJECT" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab,BpGeneratorUltimate \
  -benchmark -fps=30 -ArmorPlateAudit -ArmorPlateAuditList="$LIST" -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep "RTS_ARMOR_AUDIT \(summary\|RESULT\|vehicle\)" "$LOG" | tail -80
