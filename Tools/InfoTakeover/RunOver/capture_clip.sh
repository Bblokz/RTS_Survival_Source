#!/bin/sh
# Records the run-over clip frames (capture_clip.py) in a throwaway DebugGame editor on RTS_ToCopySmall: a player
# Tiger H1 drives through an enemy Soviet rifle squad. Never the shared editor; nothing is saved.
# usage: sh capture_clip.sh [timeout_s]     env: RUN_OVER_X/Y, RUN_OVER_START (see the .py)
# Output: Saved/InfoTakeover/RunOver/Frames, FramesSide, FramesWide/Frame_<n>.png (1280x720, one per 1/24 game s)
#         + capture.json; log Saved/Logs/RunOverClip.log. Then ../VulturesAt/make_clip_atlas.py --clip RunOver.
export MSYS_NO_PATHCONV=1
OUT=D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/RunOver
LOG=D:/UE5Projects/RTS_Survival/Saved/Logs/RunOverClip.log
LIMIT="${1:-900}"
rm -rf "$OUT/Frames" "$OUT/FramesSide" "$OUT/FramesWide"
mkdir -p "$OUT"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe" \
  D:/UE5Projects/RTS_Survival/RTS_Survival.uproject \
  "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" \
  -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False \
  -ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False \
  -ini:Game:[/Script/RTS_Survival.RTSVehicleCrushSettings]:DodgeChance=0.0 \
  -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/InfoTakeover/RunOver/capture_clip.py" \
  -benchmark -fps=24 -ResX=1280 -ResY=720 -nosound -unattended -nosplash \
  -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing own editor"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep -E "RUN_OVER_CLIP|LogPython: Error|Error: " $LOG | cut -c1-400 | tail -80
