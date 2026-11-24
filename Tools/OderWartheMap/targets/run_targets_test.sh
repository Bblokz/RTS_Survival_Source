# Throwaway DebugGame editor on RTS_ToCopySmall (never a mission map, never the shared editor); PIE test of the targets.
# usage: sh run_targets_test.sh <run name> [timeout_s]
export MSYS_NO_PATHCONV=1
export TARGET_RUN="$1"
OUTDIR=D:/UE5Projects/RTS_Survival/Saved/OderWarthe/targets/test/$1
mkdir -p $OUTDIR
LOG=$OUTDIR/editor.log
LIMIT="${2:-600}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/${TARGET_EXE:-UnrealEditor-Win64-DebugGame.exe}" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/OderWartheMap/targets/${TARGET_SCRIPT:-ow_targets_test.py}" -ResX=1280 -ResY=720 ${TARGET_EXTRA} -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing own editor"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep -E "OWTARGETTEST|LogPython: Error|Ensure" $LOG | cut -c1-500 | tail -80
