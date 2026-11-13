#!/bin/sh
# Runs one of the furniture sound scripts in a headless commandlet (no window, no renderer, no UECP endpoint).
# usage: run_headless.sh furniture_sounds_export.py|furniture_sounds_import.py [logname]
export MSYS_NO_PATHCONV=1
DIR=D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/furniture/sounds
OUT=D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/sounds
mkdir -p $OUT
LOG=$OUT/${2:-${1%.py}}.log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject -run=pythonscript -script=$DIR/$1 -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate -abslog=$LOG > /dev/null 2>&1
echo "exit $?"
# exit 1 with a "Decoder for AudioFormat BINKA not found" ensure is harmless (no audio decoders in a commandlet)
grep -E "FURN_SND|Traceback|LogPython: Error" $LOG | tail -40
