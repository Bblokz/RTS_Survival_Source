#!/bin/sh
# Usage: run_py.sh <script.py> [tanks] [dry tanks]
#   tanks: comma separated names from tank_config.py (empty = all).
#   dry tanks (build_tank_pieces.py only): names, or ALL, whose parts are only listed.
# Runs one python file of this folder in a headless commandlet (no GPU). One run at a time.
ROOT="D:/UE5Projects/RTS_Survival"
NAME=$(basename "$1" .py)
export DIVISIONTANKS="$2"
export DIVISIONTANKS_DRY="$3"
mkdir -p "$ROOT/Saved/WorldDivisionPieces"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "$ROOT/RTS_Survival.uproject" -run=pythonscript -script="$ROOT/Source/Tools/WorldDivisionPieces/$NAME.py" \
  -nullrhi -unattended -nop4 -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate \
  -abslog="$ROOT/Saved/WorldDivisionPieces/$NAME.log" > /dev/null 2>&1
echo "exit=$?"
grep -E "Error|Traceback|DIVTANK" "$ROOT/Saved/WorldDivisionPieces/$NAME.log" | grep -v -E "LogShaderCompilers|LogAudio|LogStreaming: Display|Message dialog|ReportError" | cut -c1-300 | tail -40
