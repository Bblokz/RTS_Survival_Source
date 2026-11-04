#!/usr/bin/env bash
# Offscreen check of the faction menu's team colour picker (-FactionMenuCaptureTeamColors): clicks red, blue, yellow
# and the camo default, screenshots each to Saved/FactionMenu/ and logs the colour the hangar preview received.
# The swatches save to the player profile, so the save games are backed up first and put back afterwards.
# UE_CMD picks the build, e.g. UnrealEditor-Win64-DebugGame-Cmd.exe while the user's editor locks the Development DLL.
set -u
PROJECT="D:/UE5Projects/RTS_Survival"
ENGINE="${UE_CMD:-D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe}"
LOG="$PROJECT/Saved/Logs/FactionMenuTeamColors.log"
SAVES="$PROJECT/Saved/SaveGames"
BACKUP="$PROJECT/Saved/TeamColorTest/savegames_backup"
rm -rf "$BACKUP"
if [ -d "$SAVES" ]; then cp -r "$SAVES" "$BACKUP"; fi
MSYS_NO_PATHCONV=1 "$ENGINE" "$PROJECT/RTS_Survival.uproject" /Game/RTS_Survival/FactionSelection/RTS_FactionSelection \
  -game -unattended -nosound -RenderOffscreen -windowed -ResX=1920 -ResY=1080 -DisablePlugins=Fab \
  -FactionMenuCapture -FactionMenuCaptureTeamColors -abslog="$LOG"
echo "exit=$? log=$LOG"
if [ -d "$BACKUP" ]; then rm -rf "$SAVES" && cp -r "$BACKUP" "$SAVES" && echo "save games restored"; fi
grep "RTS_FACTION_MENU_CAPTURE" "$LOG" | sed 's/.*RTS_FACTION_MENU_CAPTURE/RTS_FACTION_MENU_CAPTURE/'
