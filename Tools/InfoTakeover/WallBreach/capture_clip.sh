#!/bin/sh
# Records the Wall Breach clip frames (capture_clip.py) in a throwaway DebugGame editor on RTS_ToCopySmall: a player
# Tiger H1 drives through a Soviet Rus_Wall_Concrete_Long. Never the shared editor; nothing is saved.
# usage: sh capture_clip.sh [timeout_s]     env: WALL_BREACH_WALL, WALL_BREACH_X/Y, WALL_BREACH_START (see the .py)
# Output: Saved/InfoTakeover/WallBreach/Frames, FramesEntry, FramesWide/Frame_<n>.png (1280x720, one per 1/24 game s)
#         + capture.json; log Saved/Logs/WallBreachClip.log. Then ../VulturesAt/make_clip_atlas.py --clip WallBreach.
export MSYS_NO_PATHCONV=1
OUT=D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/WallBreach
LOG=D:/UE5Projects/RTS_Survival/Saved/Logs/WallBreachClip.log
LIMIT="${1:-900}"
rm -rf "$OUT/Frames" "$OUT/FramesEntry" "$OUT/FramesWide"
mkdir -p "$OUT"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe" \
  D:/UE5Projects/RTS_Survival/RTS_Survival.uproject \
  "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" \
  -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False \
  -ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False \
  -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/InfoTakeover/WallBreach/capture_clip.py" \
  -benchmark -fps=24 -ResX=1280 -ResY=720 -nosound -unattended -nosplash \
  -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing own editor"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep -E "WALL_BREACH_CLIP|LogPython: Error|Error: " $LOG | cut -c1-400 | tail -80
