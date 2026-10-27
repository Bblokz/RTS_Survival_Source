#!/bin/bash
# Usage: run_toss_demo.sh <label> [extra args, e.g. -TurretTossAudit or -TurretTossDemoTank=Tank_Tiger]  -> frames in Saved/TurretTossDemo/<label>, log Saved/Logs/TurretTossDemo_<label>.log
LABEL=$1; shift
cd /d/UE5Projects/RTS_Survival || exit 1
OUT=D:/UE5Projects/RTS_Survival/Saved/TurretTossDemo/$LABEL
rm -rf "Saved/TurretTossDemo/$LABEL"; mkdir -p "Saved/TurretTossDemo/$LABEL"
MSYS_NO_PATHCONV=1 timeout 1500 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nop4 \
  -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab -benchmark -fps=30 "-ExecCmds=r.TextureStreaming 0" \
  -TurretTossDemo "-TurretTossDemoDir=$OUT" "-TurretTossDemoAt=16796,-15808" "$@" \
  "-abslog=D:/UE5Projects/RTS_Survival/Saved/Logs/TurretTossDemo_$LABEL.log" > /dev/null 2>&1
echo "exit $?"
grep -a -E "RTS_TTD|RTS_TTA|WreckTurretToss" "Saved/Logs/TurretTossDemo_$LABEL.log" | sed 's/^.*Display: //' | awk 'NR<=8 || /RESULT|SUMMARY|RTS_TTA/ || NR%4==0' | head -400
echo "frames: $(ls Saved/TurretTossDemo/$LABEL | wc -l)"
