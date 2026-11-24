#!/usr/bin/env bash
# Plain headless -game playthrough of the intro cinematic map (no Movie Render Queue): the map's own sequence actor
# plays the master, the film quits the game at its end, and a screenshot with UI is taken every <seconds> (default 10).
#   game_run.sh [seconds between screenshots]
# Evidence: Saved/IntroCinematic/game_run.log (INTRO_CINEMATIC lines) and Saved/IntroCinematic/GameRun/*.png
set -u
EVERY="${1:-10}"
ROOT=D:/UE5Projects/RTS_Survival
rm -rf "$ROOT/Saved/IntroCinematic/GameRun"
MSYS_NO_PATHCONV=1 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "$ROOT/RTS_Survival.uproject" /Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976 -game -RenderOffscreen -windowed \
  -ResX=1280 -ResY=720 -IntroCinematicAutoQuit -IntroCinematicShotEvery=$EVERY \
  "-ini:Game:[/Script/RTS_Survival.RTSCoverFinderDeveloperSettings]:bM_EnableCoverSearch=False" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding \
  "-abslog=$ROOT/Saved/IntroCinematic/game_run.log" > /dev/null 2>&1
echo "game run exit $?, screenshots: $(ls "$ROOT/Saved/IntroCinematic/GameRun" 2>/dev/null | wc -l)"
grep "INTRO_CINEMATIC\|LogLoad: Game class\|Fatal error" "$ROOT/Saved/IntroCinematic/game_run.log" | cut -c1-200
