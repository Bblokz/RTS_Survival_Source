#!/bin/sh
# Launches furniture_sound_test.py in a throwaway editor on RTS_ToCopySmall (never Ger_Berlin, never the shared editor).
# A real editor window with an audio device: the "New ActiveSound" log lines only exist when sounds really start.
# usage: furniture_sound_run.sh <logname> [timeout_s]      (a run takes about 2.5 minutes; default limit 360 s)
export MSYS_NO_PATHCONV=1
mkdir -p D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/sounds
LOG=D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/sounds/$1.log
LIMIT="${2:-360}"
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/furniture/sounds/furniture_sound_test.py" -LogCmds="LogAudio VeryVerbose, LogAudioConcurrency Verbose, LogWallHoleDamage Verbose" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG &
PID=$!
WAITED=0
while kill -0 $PID 2>/dev/null; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge $LIMIT ]; then echo "timeout, killing"; taskkill //F //PID $(ps -p $PID -o winpid= 2>/dev/null) 2>/dev/null; kill $PID; break; fi
done
echo "exit after ${WAITED}s"
grep -E "FURNSOUND .*(verdict|PASSED|ERROR|timeout|runtime|listener)" $LOG | cut -c1-420 | tail -45
