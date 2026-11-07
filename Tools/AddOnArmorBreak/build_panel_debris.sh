#!/usr/bin/env bash
# Builds NS_AddOnArmorPanelDebris and gives every built panel hull a debris panel (AddOnArmorPanels/AGENTS.md).
# Hulls whose panels were modelled before the debris rule get one left panel cut out of the hull
# (RTS.AddOnArmor.ExtractPanelDebris); a modelled panel replaces it with RTS.AddOnArmor.SetPanelDebrisMesh.
# Runs headless; no editor may have the hulls or their panel materials open (saves then fail with error 32).
# Usage: build_panel_debris.sh            system + every *_AddOnPanels hull under TrackBasedVehicle
#        ONLY="<hull name> ..." build_panel_debris.sh    only those hulls (no system)
set -u
PROJECT="${PROJECT:-D:/UE5Projects/RTS_Survival/RTS_Survival.uproject}"
ENGINE="${ENGINE:-D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe}"
CONTENT="D:/UE5Projects/RTS_Survival/Content"
VEHICLES="$CONTENT/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle"
LOG="D:/UE5Projects/RTS_Survival/Saved/Logs/AddOnArmorPanelDebris.log"

COMMANDS=()
if [ -z "${ONLY:-}" ]; then
  COMMANDS+=("RTS.AddOnArmor.BuildDebrisSystem")
fi
while IFS= read -r FILE; do
  NAME="$(basename "$FILE" .uasset)"
  if [ -n "${ONLY:-}" ] && [[ " $ONLY " != *" $NAME "* ]]; then
    continue
  fi
  PACKAGE="/Game${FILE#"$CONTENT"}"
  COMMANDS+=("RTS.AddOnArmor.ExtractPanelDebris Hull=${PACKAGE%.uasset}")
done < <(find "$VEHICLES" -name "SM_*_AddOnPanels.uasset" | sort)
COMMANDS+=("QUIT_EDITOR")

JOINED="$(printf '%s, ' "${COMMANDS[@]}")"
MSYS_NO_PATHCONV=1 "$ENGINE" "$PROJECT" /Engine/Maps/Entry -ExecCmds="${JOINED%, }" \
  -unattended -nullrhi -nosplash -nosound -DisablePlugins=Fab,BpGeneratorUltimate -abslog="$LOG"
echo "exit=$? log=$LOG"
grep "RTS_ADDON_DEBRIS_AUTHORING" "$LOG" | grep -v "compiled script" | tail -60
