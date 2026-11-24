#!/usr/bin/env bash
# Records the Wespe Barrage clip frames (UWespeBarrageClipCaptureSubsystem, -WespeBarrageClipCapture) in an offscreen
# DebugGame game run: three player Wespes fire the Barrage aim ability at a T-26 and a Mosin squad 4500 out.
# Usage: capture_clip.sh [extra UE switches, e.g. -AbilityClipAt=1200,-300]
#   MAP=/Game/... capture_clip.sh       map to film on (default RTS_ToCopySmall at 0,0)
# Output: Saved/InfoTakeover/WespeBarrage/Frames (battery camera) and FramesTarget (target camera), one PNG per
#         1/24 s tick each, log Saved/Logs/WespeBarrageClip.log. Then run ../VulturesAt/make_clip_atlas.py
#         --clip WespeBarrage with Target: segments for the target camera.
set -u
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
SAVED="D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/WespeBarrage"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/WespeBarrageClip.log"
rm -rf "$SAVED/Frames" "$SAVED/FramesTarget"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -benchmark -fps=24 -ExecCmds="r.TextureStreaming 0" -WespeBarrageClipCapture -AbilityClipAt=0,0 \
  -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "WESPE_BARRAGE_CLIP" "$LOG" | sed "s/.*WESPE_BARRAGE_CLIP //"
