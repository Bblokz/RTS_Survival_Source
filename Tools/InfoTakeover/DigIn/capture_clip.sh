#!/usr/bin/env bash
# Records the Dig In clip frames (UDigInClipCaptureSubsystem, -DigInClipCapture) in an offscreen DebugGame game run:
# a player Panzer IV H digs in facing a parked enemy T-26, then the T-26 opens fire on it.
# Usage: capture_clip.sh [extra UE switches, e.g. -AbilityClipAt=1200,-300]
#   MAP=/Game/... capture_clip.sh       map to film on (default RTS_ToCopySmall at 0,0, where units drive cleanly)
# Output: Saved/InfoTakeover/DigIn/Frames + FramesWide/Frame_<n>.png (1280x720, one per 1/24 s tick),
#         log Saved/Logs/DigInClip.log. Then run ../VulturesAt/make_clip_atlas.py --clip DigIn.
set -u
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
OUT="D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/DigIn"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/DigInClip.log"
rm -rf "$OUT"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -benchmark -fps=24 -ExecCmds="r.TextureStreaming 0" -DigInClipCapture \
  -AbilityClipAt=0,0 -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "DIGIN_CLIP" "$LOG" | sed "s/.*DIGIN_CLIP //"
