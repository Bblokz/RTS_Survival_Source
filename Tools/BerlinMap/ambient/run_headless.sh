#!/bin/sh
# Runs one python file in a headless commandlet editor (no window, no RHI, no UECP endpoint): asset builds only.
# usage: run_headless.sh <script.py> [logname] [script args...]
export MSYS_NO_PATHCONV=1
SCRIPT="$1"; NAME="${2:-headless}"; shift; shift
LOG=D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/logs/$NAME.log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject -run=pythonscript -script="$SCRIPT $*" -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate -abslog=$LOG > /dev/null 2>&1
echo "exit=$? log=$LOG"
grep -E "AMB_|LogPython: Error|RTS_IMPACT_VFX_AUTHORING" "$LOG" | grep -v "LogInit: Display" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -${LINES_MAX:-80}
