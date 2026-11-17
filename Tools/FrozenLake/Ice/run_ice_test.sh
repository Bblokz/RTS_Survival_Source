#!/usr/bin/env bash
# Throwaway DebugGame editor running ice_headless_test.py (builds / updates the ice assets in place, shoots).
#   bash run_ice_test.sh        on RTS_ToCopySmall (test lake only)
#   bash run_ice_test.sh map    on Ger_FrozenLake: the real lake in winter light + the test lake floating 300 m up
#   bash run_ice_test.sh tracks on Ger_FrozenLake: vehicle tracks only (hand-painted RT_TankTrails), no test lake
# Pictures + test_result.json + build_report.json land in Saved/FrozenLake/ice/. Never saves the map.
# Only run when no other editor has M_FrozenLakeIce / its instances / Ger_FrozenLake loaded.
set -u
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe"
SCRIPT="D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLake/Ice/ice_headless_test.py"
MAP="/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/FrozenLakeIce_Test.log"
if [ "${1:-}" = "tracks" ]; then
  export ICE_TEST_TRACKS=1
fi
if [ "${1:-}" = "map" ] || [ "${1:-}" = "tracks" ]; then
  MAP="/Game/RTS_Survival/Maps/GerMissions/Ger_FrozenLake"
  LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/FrozenLakeIce_TestMap.log"
  export ICE_TEST_ON_MAP=1
fi
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" \
  "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=$MAP" \
  -ExecCmds="py $SCRIPT" -unattended -nosplash -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate \
  -ResX=960 -ResY=540 -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "FROZENICE|Failed to compile|missing input" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | cut -c1-400 | head -80
