#!/bin/sh
# Runs import_aircraft_gun_sounds.py in a headless DebugGame commandlet (no window, no renderer).
# The DebugGame editor build must contain UAircraftGunAudioDataAsset. Do not run while an editor has these assets open.
export MSYS_NO_PATHCONV=1
DIR=D:/UE5Projects/RTS_Survival/Source/Tools/AircraftStrafeAudio
OUT=D:/UE5Projects/RTS_Survival/Saved/AircraftGunAudio
mkdir -p $OUT
LOG=$OUT/import.log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
	D:/UE5Projects/RTS_Survival/RTS_Survival.uproject -run=pythonscript -script=$DIR/import_aircraft_gun_sounds.py \
	-unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate -abslog=$LOG > /dev/null 2>&1
echo "exit $?"
# exit 1 with a "Decoder for AudioFormat BINKA not found" ensure is harmless (no audio decoders in a commandlet)
grep -E "AIRCRAFT_GUN_SND|Traceback|LogPython: Error" $LOG | tail -40
