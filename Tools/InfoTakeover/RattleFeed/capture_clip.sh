#!/usr/bin/env bash
# Records the Rattle-Feed clip frames (UBehaviourAbilityClipCaptureSubsystem, -RattleFeedClipCapture) in an offscreen
# DebugGame game run: a player Panzer II F fires at a parked enemy T-26, then switches Rattle-Feed on.
# Usage: capture_clip.sh [extra UE switches, e.g. -AbilityClipAt=1200,-300]
#   MAP=/Game/... capture_clip.sh       map to film on (default RTS_ToCopySmall at 0,0, where units drive cleanly)
# Output: Saved/InfoTakeover/RattleFeed/Frames/Frame_<n>.png (1280x720, one per 1/24 s tick),
#         log Saved/Logs/RattleFeedClip.log. Then run ../VulturesAt/make_clip_atlas.py --clip RattleFeed.
set -u
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
OUT="D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/RattleFeed/Frames"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/RattleFeedClip.log"
rm -rf "$OUT"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -benchmark -fps=24 -ExecCmds="r.TextureStreaming 0" -RattleFeedClipCapture -AbilityClipDir="$OUT" \
  -AbilityClipAt=0,0 -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "ABILITY_CLIP" "$LOG" | sed "s/.*ABILITY_CLIP //"
