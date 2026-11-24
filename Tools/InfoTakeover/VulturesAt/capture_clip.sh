#!/usr/bin/env bash
# Records the Vultures AT stick grenade clip frames (UVulturesAtClipCaptureSubsystem, -VulturesAtClipCapture) in an
# offscreen DebugGame game run: a player Vulture squad throws at an enemy T-26 driving past.
# Usage: capture_clip.sh [extra UE switches, e.g. -VulturesAtClipAt=1200,-300]
#   MAP=/Game/... capture_clip.sh       map to film on (default Ger_Trains, a dressed unit-test map)
# Output: Saved/InfoTakeover/VulturesAt/Frames/Frame_<n>.png (1280x720, one per 1/24 s tick),
#         log Saved/Logs/VulturesAtClip.log. Then run make_clip_atlas.py.
set -u
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/Ger_Trains}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
OUT="D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/VulturesAt/Frames"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/VulturesAtClip.log"
rm -rf "$OUT"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -benchmark -fps=24 -ExecCmds="r.TextureStreaming 0" -VulturesAtClipCapture -VulturesAtClipDir="$OUT" \
  -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "VULTURES_AT_CLIP" "$LOG" | sed 's/.*VULTURES_AT_CLIP //'
