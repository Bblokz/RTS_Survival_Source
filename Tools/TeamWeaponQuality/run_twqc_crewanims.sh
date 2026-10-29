#!/bin/bash
# Crew animation audit of team weapons without a GPU (-nullrhi): deploy, fire until weapon 0 reloaded 3 times,
# pack, push out and back. No pictures; the poses go to CSVs that plot_crew_poses.py draws on the CPU.
# Usage: run_twqc_crewanims.sh <label> <comma separated squad subtypes> [extra test args...]
#   run_twqc_crewanims.sh at Squad_Ger_PaK40,Squad_Rus_Zis_76MM
# Log: Saved/Logs/TWCrewAnims_<label>.log, CSVs: Saved/TWCrewAnims/<label>/, crew lines: Saved/TWCrewAnims/<label>_crew.txt
LABEL=$1; SQUADS=$2; shift 2
PROJECT_DIR=/d/UE5Projects/RTS_Survival
EDITOR=/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe
MAP=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
cd "$PROJECT_DIR" || exit 1
OUT="Saved/TWCrewAnims/$LABEL"
rm -rf "$OUT"; mkdir -p "$OUT"
MSYS_NO_PATHCONV=1 timeout 2700 "$EDITOR" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" -game -unattended -nop4 \
  -nullrhi -nosound -DisablePlugins=Fab -benchmark -fps=30 \
  -TeamWeaponQualityValidate -TWQualityAt=16796,-15808 -TWQualitySearchCm=0 "-TWQualitySquads=$SQUADS" \
  -TWQualityTows=None:None -TWQualityCrewAnims "-TWQualityCrewAnimDir=D:/UE5Projects/RTS_Survival/$OUT" \
  "-LogCmds=LogAnimMontage Verbose" \
  "$@" "-abslog=D:/UE5Projects/RTS_Survival/Saved/Logs/TWCrewAnims_$LABEL.log" > /dev/null 2>&1
echo "$LABEL exit $?"
grep "RTS_TWQC" "Saved/Logs/TWCrewAnims_$LABEL.log" | sed 's/^.*: Display: //' > "Saved/TWCrewAnims/${LABEL}_crew.txt"
grep "RESULT" "Saved/TWCrewAnims/${LABEL}_crew.txt"
