#!/bin/sh
# Headless Oder-Warthe kit import (one run at a time). Jobs: Saved/OderWarthe/assets/import_jobs.json
cd /d/UE5Projects/RTS_Survival || exit 1
"/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" -run=pythonscript \
  -script="D:/UE5Projects/RTS_Survival/Source/Tools/OderWartheMap/assets/ow_kit_import.py" \
  -unattended -nullrhi -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding \
  > Saved/OderWarthe/assets/import_run.log 2>&1
echo "exit $?"
grep -E "OWKIT|Error:|Traceback" Saved/OderWarthe/assets/import_run.log | grep -v "LogInit: Display" | head -60
