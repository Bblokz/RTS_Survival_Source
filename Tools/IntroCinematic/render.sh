#!/usr/bin/env bash
# Headless Movie Render Queue render of an intro cinematic sequence.
#   render.sh <preset name> [sequence asset path]     (default sequence: the master)
# Frames land in Saved/IntroCinematic/Render/<preset name>/f.<frame>.jpeg ; log in Saved/IntroCinematic/render_<preset>.log
set -u
PRESET="$1"
SEQ="${2:-/Game/RTS_Survival/Cinematics/Intro1976/LS_Intro1976_Master}"
ROOT=D:/UE5Projects/RTS_Survival
rm -rf "$ROOT/Saved/IntroCinematic/Render/$PRESET"
MSYS_NO_PATHCONV=1 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "$ROOT/RTS_Survival.uproject" /Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976 -game -RenderOffscreen -windowed \
  -ResX=1280 -ResY=720 -LevelSequence="$SEQ" -MoviePipelineConfig="/Game/RTS_Survival/Cinematics/Intro1976/Render/$PRESET" \
  "-ini:Game:[/Script/RTS_Survival.RTSCoverFinderDeveloperSettings]:bM_EnableCoverSearch=False" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -nosound \
  "-abslog=$ROOT/Saved/IntroCinematic/render_$PRESET.log" > /dev/null 2>&1
CODE=$?
echo "render exit $CODE, frames: $(ls "$ROOT/Saved/IntroCinematic/Render/$PRESET" 2>/dev/null | wc -l)"
grep "INTRO_CINEMATIC\|Movie Pipeline completed\|Fatal error" "$ROOT/Saved/IntroCinematic/render_$PRESET.log" | cut -c1-220 | sort | uniq -c | head -20
# A cinematic material that fails here draws as the grey default material: this count must be 0.
echo "broken cinematic materials: $(grep "Intro1976.*Failed to compile" "$ROOT/Saved/IntroCinematic/render_$PRESET.log" | grep -o "Materials.M[A-Za-z_]*" | sort -u | xargs)"
exit $CODE
