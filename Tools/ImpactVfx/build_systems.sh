#!/usr/bin/env bash
# Builds the impact Niagara systems a recipe lists (RTS.ImpactVfx.BuildFromRecipe) in a headless editor and saves them.
# Usage: build_systems.sh [recipe.json]   (default: howitzer_impacts.json next to this script)
# The open editor must not have the target systems loaded (the save fails on a locked file); unload them there first
# (unreal.EditorLoadingAndSavingUtils.unload_packages) and reload them afterwards.
# Needs the Development editor target built (the user's editor runs DebugGame, so there is no DLL lock).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
RECIPE="$(cygpath -m "$(realpath "${1:-$HERE/howitzer_impacts.json}")")"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/ImpactVfxBuild.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" /Engine/Maps/Entry \
  -ExecCmds="RTS.ImpactVfx.BuildFromRecipe $RECIPE, QUIT_EDITOR" -unattended -nullrhi -nosound -DisablePlugins=Fab \
  -abslog="$LOG"
echo "exit=$? log=$LOG"
grep "RTS_IMPACT_VFX_AUTHORING\|Illegal reference\|Failed to save\|Error: .*Niagara" "$LOG" | tail -80
