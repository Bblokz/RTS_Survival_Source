#!/bin/sh
# Applies fence_breach_setup.py in a DebugGame commandlet (needs the DebugGame DLL with RTS.WallBreach.ConfigureRusWall).
export MSYS_NO_PATHCONV=1
OUTDIR=D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach
mkdir -p $OUTDIR
LOG=$OUTDIR/fence_setup.log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject -run=pythonscript -script="D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Breach/fence_breach_setup.py" -unattended -nosplash -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -abslog=$LOG > /dev/null 2>&1
echo "exit=$?"
grep -E "RTS_WALL_BREACH_SETUP|FENCE_BREACH|WALL_FIELD|ConfigureRusWall|Error: |LogPython: Error|Traceback" $LOG | cut -c1-400 | tail -40
