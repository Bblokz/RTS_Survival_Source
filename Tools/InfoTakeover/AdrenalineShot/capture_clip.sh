#!/usr/bin/env bash
# Records the Adrenaline Shot clip frames (UBehaviourAbilityClipCaptureSubsystem, -AdrenalineShotClipCapture) in an offscreen
# DebugGame game run: a player Panzer 38(t) fires at a parked enemy T-26, then uses Adrenaline Shot.
# Usage: capture_clip.sh [extra UE switches, e.g. -AbilityClipAt=1200,-300]
#   MAP=/Game/... capture_clip.sh       map to film on (default RTS_ToCopySmall at 0,0, where units drive cleanly)
# Output: Saved/InfoTakeover/AdrenalineShot/Frames/Frame_<n>.png (1280x720, one per 1/24 s tick),
#         log Saved/Logs/AdrenalineShotClip.log. Then run ../VulturesAt/make_clip_atlas.py --clip AdrenalineShot.
set -u
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
OUT="D:/UE5Projects/RTS_Survival/Saved/InfoTakeover/AdrenalineShot/Frames"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/AdrenalineShotClip.log"
rm -rf "$OUT"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab \
  -benchmark -fps=24 -ExecCmds="r.TextureStreaming 0" -AdrenalineShotClipCapture -AbilityClipDir="$OUT" \
  -AbilityClipAt=0,0 -abslog="$LOG" "$@"
echo "exit=$? log=$LOG"
grep -E "ABILITY_CLIP" "$LOG" | sed "s/.*ABILITY_CLIP //"
