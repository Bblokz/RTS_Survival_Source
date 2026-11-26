#!/bin/sh
# Throwaway Ger_Berlin editor for the Glasherz pass (own command folder Saved/BerlinGZC/cmd), -nullrhi: no GPU use,
# because the shared 12 GB card is nearly full (see memory gpu-oom-crashes-other-editors). Never saves by itself.
# Quit: python gzc_cmd.py -c "unreal.SystemLibrary.quit_editor()"
export MSYS_NO_PATHCONV=1
MAP="${1:-/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin}"
CMD=D:/UE5Projects/RTS_Survival/Saved/BerlinGZC/cmd
mkdir -p $CMD D:/UE5Projects/RTS_Survival/Saved/BerlinGZC/logs
rm -f $CMD/server_ready.txt $CMD/req_* $CMD/res_* $CMD/tmp_*
LOG=D:/UE5Projects/RTS_Survival/Saved/BerlinGZC/logs/editor_$(date +%m%d_%H%M).log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=$MAP" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/glasherz/gzc_editor_server.py" -nullrhi -unattended -nosplash -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=$LOG > /dev/null 2>&1 &
echo "started pid $! log $LOG"
WAITED=0
while [ ! -f $CMD/server_ready.txt ]; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge 900 ]; then echo "server not ready after 900 s"; exit 1; fi
done
echo "ready after ${WAITED}s, editor pid $(cat $CMD/server_ready.txt)"
