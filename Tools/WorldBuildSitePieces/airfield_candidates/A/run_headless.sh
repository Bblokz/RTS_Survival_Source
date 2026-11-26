#!/bin/sh
# Usage: run_headless.sh <script name without .py>   Runs one script of this folder in a headless DebugGame commandlet.
ROOT="D:/UE5Projects/RTS_Survival"
NAME="$1"
OUT="$ROOT/Saved/WorldBuildSitePieces/airfield_candidate_A"
mkdir -p "$OUT"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "$ROOT/RTS_Survival.uproject" -run=pythonscript -script="$ROOT/Source/Tools/WorldBuildSitePieces/airfield_candidates/A/$NAME.py" \
  -nullrhi -unattended -nop4 -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate \
  -abslog="$OUT/$NAME.log" > /dev/null 2>&1
echo "exit=$?"
grep -E "Error|Traceback|AIRFIELD_A" "$OUT/$NAME.log" | grep -v -E "LogShaderCompilers|LogAudio|LogStreaming: Display|Message dialog|ReportError" | cut -c1-300 | tail -40
