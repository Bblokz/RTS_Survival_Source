#!/usr/bin/env bash
# Rebuilds the emitter scripts of NS_AmbientDustDevils from Source/RTS_Survival/Environment/AmbientFx/DustDevilHlsl
# in a headless editor (Docs/Environment/AmbientFx.md). An open editor must not hold the system: close its Niagara
# tab, or unload the package, first, or the save fails with error 32.
#   BIN=Development patch_dust_devils.sh     default DebugGame (the shared editor holds the Development DLL)
set -u
HERE="D:/UE5Projects/RTS_Survival/Source/Tools/AmbientFx"
if [ "${BIN:-DebugGame}" = "Development" ]; then
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
else
  ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
fi
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/AmbientFx_Patch.log"
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="$HERE/ambient_fx_job.py patch" -unattended -nosplash -NoSound -DisablePlugins=Fab -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_DUST_DEVIL_AUTHORING" "$LOG" | sed 's/.*RTS_DUST_DEVIL_AUTHORING //' | head -60
