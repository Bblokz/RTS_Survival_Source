# Throwaway DebugGame editor, RTS_ToCopySmall unless BREACH_MAP names a map for a read-only replay (never the shared editor).
# Throwaway DebugGame editor on RTS_ToCopySmall (never a mission map, never the shared editor).
# usage: sh run_breach_test.sh <run name> [timeout_s]   script: BREACH_SCRIPT (default wall_breach_test.py); map: BREACH_MAP (read-only replays only, never saved)
export MSYS_NO_PATHCONV=1
export BREACH_RUN="$1"
OUTDIR=D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach/$1
mkdir -p $OUTDIR
LOG=$OUTDIR/editor.log
LIMIT="${2:-600}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/${BREACH_EXE:-UnrealEditor-Win64-DebugGame.exe}" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=${BREACH_MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}" -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Breach/${BREACH_SCRIPT:-wall_breach_test.py}" -LogCmds="LogWallHoleDamage Verbose, LogWallTankBreach Verbose" -ResX=1280 -ResY=720 -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing own editor"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep -E "BREACHTEST|RTS_WALL_BREACH|Error: |Ensure|LogPython: Error" $LOG | cut -c1-600 | tail -90
