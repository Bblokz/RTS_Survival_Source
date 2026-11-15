#!/bin/bash
# Renders the supply point Blueprint picture in a throwaway DebugGame editor under the shared DebugGame lock.
LOCK="D:/UE5Projects/RTS_Survival/Saved/AgentLocks/debuggame_build.lockdir"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/SupplyTruckBP/render_supply_point_bp.log"
for i in $(seq 1 120); do
  if mkdir "$LOCK" 2>/dev/null; then GOT=1; break; fi
  sleep 5
done
if [ -z "$GOT" ]; then echo "LOCK_TIMEOUT"; exit 3; fi
trap 'rmdir "$LOCK"' EXIT
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" \
  "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" \
  -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Model/ue_render_supply_point_bp.py" \
  -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -ResX=1280 -ResY=720 -abslog="$LOG" > /dev/null 2>&1
echo "EXIT $?"
