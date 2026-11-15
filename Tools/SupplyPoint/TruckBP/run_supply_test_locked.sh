#!/bin/bash
# Runs run_supply_point_test.ps1 under the shared DebugGame lock. Extra args are passed through.
LOCK="D:/UE5Projects/RTS_Survival/Saved/AgentLocks/debuggame_build.lockdir"
for i in $(seq 1 120); do
  if mkdir "$LOCK" 2>/dev/null; then GOT=1; break; fi
  sleep 5
done
if [ -z "$GOT" ]; then echo "LOCK_TIMEOUT"; exit 3; fi
trap 'rmdir "$LOCK"' EXIT
MSYS_NO_PATHCONV=1 powershell -NoProfile -File "D:\UE5Projects\RTS_Survival\Source\Tools\SupplyPoint\run_supply_point_test.ps1" "$@"
