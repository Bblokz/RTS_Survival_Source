#!/bin/sh
# Builds M_UI_SignalLamp + MI_InfoTakeover_PageLamp headless (Development commandlet, never the open editor).
export MSYS_NO_PATHCONV=1
ROOT=D:/UE5Projects/RTS_Survival
LOG=$ROOT/Saved/Logs/PageLampsBuild.log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $ROOT/RTS_Survival.uproject \
  -run=pythonscript -script=$ROOT/Source/Tools/InfoTakeover/Style/build_page_lamps.py -unattended -nullrhi -nosplash \
  -DisablePlugins=Fab,BpGeneratorUltimate -abslog=$LOG > /dev/null 2>&1
grep -E "PAGE_LAMPS|LogPython: Error" $LOG | cut -c1-300
