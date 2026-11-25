#!/bin/sh
# Usage: run_build.sh <script.py> [pieces]   (pieces: comma separated, passed as WORLDPIECES)
ROOT="D:/UE5Projects/RTS_Survival"
SCRIPT="$1"
NAME=$(basename "$SCRIPT" .py)
export WORLDPIECES="$2"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "$ROOT/RTS_Survival.uproject" -run=pythonscript -script="$ROOT/Source/Tools/WorldMapPieces/$NAME.py" \
  -nullrhi -unattended -nop4 -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate \
  -abslog="$ROOT/Saved/WorldMapPieces/$NAME.log" > /dev/null 2>&1
echo "exit=$?"
grep -E "Error|Traceback|WORLDPIECE" "$ROOT/Saved/WorldMapPieces/$NAME.log" | grep -v -E "LogShaderCompilers|LogAudio|LogStreaming: Display|Message dialog|ReportError" | cut -c1-300 | tail -80
