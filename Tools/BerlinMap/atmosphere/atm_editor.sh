#!/bin/sh
# Starts MY throwaway editor on a map (default Ger_Berlin) with the atmosphere command server (own command folder:
# Saved/Berlin/atmosphere/cmd, so it never takes another session's requests). Never saves anything by itself.
# usage: atm_editor.sh [map package]    then: python atm_cmd.py <file.py>; quit with: python atm_cmd.py -c "unreal.SystemLibrary.quit_editor()"
export MSYS_NO_PATHCONV=1
MAP="${1:-/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin}"
CMD=D:/UE5Projects/RTS_Survival/Saved/Berlin/atmosphere/cmd
mkdir -p $CMD D:/UE5Projects/RTS_Survival/Saved/Berlin/atmosphere/logs
rm -f $CMD/server_ready.txt $CMD/req_* $CMD/res_* $CMD/tmp_*
LOG=D:/UE5Projects/RTS_Survival/Saved/Berlin/atmosphere/logs/editor.log
"D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=$MAP" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/atmosphere/atm_editor_server.py" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -ResX=1600 -ResY=900 -abslog=$LOG > /dev/null 2>&1 &
echo "started pid $!"
WAITED=0
while [ ! -f $CMD/server_ready.txt ]; do
  sleep 5; WAITED=$((WAITED+5))
  if [ $WAITED -ge 600 ]; then echo "server not ready after 600 s"; exit 1; fi
done
echo "ready after ${WAITED}s, editor pid $(cat $CMD/server_ready.txt)"
