#!/usr/bin/env bash
# Pictures the three flare Niagara systems offscreen (UImpactVfxCaptureSubsystem) and builds one contact sheet per
# system (side view on top, game camera below). Uses the DebugGame -Cmd binary, which can run next to the open editor.
# Usage: capture_flare_vfx.sh <round name>
set -u
NAME="${1:-latest}"
OUT="D:/UE5Projects/RTS_Survival/Saved/FlareVfx/$NAME"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/FlareVfxCapture.log"
FLARE=/Game/RTS_Survival/SpecialEffects/Flare
rm -rf "$OUT"
MSYS_NO_PATHCONV=1 "D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" /Game/RTS_Survival/Maps/UnitTests/UT_B4Howitzer \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab -benchmark -fps=30 \
  -ExecCmds="r.TextureStreaming 0" -ImpactVfxSkyFill=3 -ImpactVfxExposureBias=1.0 -ImpactVfxScale=1.0 \
  -ImpactVfxMoments=0.05,0.3,1.0,2.5 "-ImpactVfxCapture=$FLARE/NS_Flare_Launch+$FLARE/NS_Flare_Burn+$FLARE/NS_Flare_Trail" \
  -ImpactVfxDir="$OUT" -abslog="$LOG"
echo "exit=$?"
grep "RTS_IMPACT_VFX.*max particles" "$LOG" | sed 's/.*RTS_IMPACT_VFX //'
cd "$OUT" || exit 1
for s in NS_Flare_Launch NS_Flare_Burn NS_Flare_Trail; do
  magick $s/${s}_Side_t00.05.png $s/${s}_Side_t00.30.png $s/${s}_Side_t01.00.png $s/${s}_Side_t02.50.png +append -resize 40% side.png
  magick $s/${s}_Game_t00.05.png $s/${s}_Game_t00.30.png $s/${s}_Game_t01.00.png $s/${s}_Game_t02.50.png +append -resize 40% game.png
  magick side.png game.png -append ${s}_sheet.png
done
rm -f side.png game.png
