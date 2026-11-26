#!/bin/sh
# Starts the crater pass's throwaway editor on Ger_Berlin with crater_editor_server.py (own command folder
# Saved/BerlinCrater/cmd). Never saves by itself. Quit: python crater_cmd.py -c "unreal.SystemLibrary.quit_editor()"
export MSYS_NO_PATHCONV=1
MAP="${1:-/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin}"
CMD=D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/cmd
mkdir -p $CMD D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/logs
rm -f $CMD/server_ready.txt $CMD/req_* $CMD/res_* $CMD/tmp_*
LOG=D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/logs/editor_$(date +%m%d_%H%M).log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=$MAP" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/crater_editor_server.py" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -ResX=1600 -ResY=900 -abslog=$LOG > /dev/null 2>&1 &
echo "started pid $! log $LOG"
WAITED=0
while [ ! -f $CMD/server_ready.txt ]; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge 900 ]; then echo "server not ready after 900 s"; exit 1; fi
done
echo "ready after ${WAITED}s, editor pid $(cat $CMD/server_ready.txt)"
