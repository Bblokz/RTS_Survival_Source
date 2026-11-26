#!/bin/sh
# Usage: run_headless.sh <script.py>   Runs one python script in a headless (-nullrhi) commandlet; prints its log lines.
ROOT="D:/UE5Projects/RTS_Survival"
SCRIPT="$1"
NAME=$(basename "$SCRIPT" .py)
LOGDIR="$ROOT/Saved/WorldBuildSitePieces/battery"
mkdir -p "$LOGDIR"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "$ROOT/RTS_Survival.uproject" -run=pythonscript -script="$ROOT/Source/Tools/WorldBuildSitePieces/battery/$NAME.py" \
  -nullrhi -unattended -nop4 -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate \
  -abslog="$LOGDIR/$NAME.log" > /dev/null 2>&1
echo "exit=$?"
grep -E "Error|Traceback|BATTERY" "$LOGDIR/$NAME.log" | grep -v -E "LogShaderCompilers|LogAudio|LogStreaming: Display|Message dialog|ReportError" | cut -c1-300 | tail -120
