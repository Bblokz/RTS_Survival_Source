#!/bin/sh
# Runs a ruin kit python script in a headless commandlet with a renderer (shader stats need one; -nullrhi gives none).
# usage: ruin_cmd_run.sh <script.py> <logname> [ENV=value ...]     log -> Saved/Berlin/ruinkit_textures/<logname>.log
# Never the shared editor. The commandlet reads assets the shared editor holds; saving them fails while it holds them
# (see BerlinRuinKitMaterial.md, "Rebuild while the shared editor is open").
export MSYS_NO_PATHCONV=1
SCRIPT="$1"; LOGNAME="$2"; shift 2
for kv in "$@"; do export "$kv"; done
case "$SCRIPT" in
  /*|?:*) ;;
  *) SCRIPT="D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/$SCRIPT" ;;
esac
LOG=D:/UE5Projects/RTS_Survival/Saved/Berlin/ruinkit_textures/$LOGNAME.log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject -run=pythonscript -script="$SCRIPT" -unattended -nosplash -AllowCommandletRendering -DisablePlugins=Fab,BpGeneratorUltimate -abslog=$LOG > /dev/null 2>&1
echo "exit $?"
grep -E "RUINKIT_|Error: |Traceback|RuntimeError" "$LOG" | grep -v "LogAudio\|LogStreaming: Warning" | cut -c1-400 | tail -n 80
