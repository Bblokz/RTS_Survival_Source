#!/bin/sh
# Usage: run_py.sh <script.py>   Runs one python file of this folder in a headless commandlet (no GPU).
ROOT="D:/UE5Projects/RTS_Survival"
NAME=$(basename "$1" .py)
mkdir -p "$ROOT/Saved/WorldBuildSitePieces/airfield_C"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "$ROOT/RTS_Survival.uproject" -run=pythonscript -script="$ROOT/Source/Tools/WorldBuildSitePieces/airfield_candidates/C/$NAME.py" \
  -nullrhi -unattended -nop4 -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate \
  -abslog="$ROOT/Saved/WorldBuildSitePieces/airfield_C/$NAME.log" > /dev/null 2>&1
echo "exit=$?"
grep -E "Error|Traceback|AIRFIELDC" "$ROOT/Saved/WorldBuildSitePieces/airfield_C/$NAME.log" | grep -v -E "LogShaderCompilers|LogAudio|LogStreaming: Display|Message dialog|ReportError" | cut -c1-300 | tail -40
