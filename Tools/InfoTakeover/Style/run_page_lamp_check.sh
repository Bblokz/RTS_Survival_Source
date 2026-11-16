#!/bin/sh
# Page lamp check: runs -CameraControlsTakeoverValidate (3 pages, so the lit lamp moves) headless in DebugGame at each
# resolution, then copies its shots to Saved/InfoTakeover/PageLamps/<WxH>/. Never touches the shared editor.
# usage: sh run_page_lamp_check.sh 1920x1080 2560x1080 3440x1440 ...
# Grep: "page lamps (" lines (row rect / lit lamp centre in plate units vs expected) and PASS/FAIL.
export MSYS_NO_PATHCONV=1
ROOT=D:/UE5Projects/RTS_Survival
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
for RES in "$@"; do
  W=${RES%x*}; H=${RES#*x}
  LOG=$ROOT/Saved/Logs/PageLampCheck_$RES.log
  rm -rf "$ROOT"/Saved/InfoTakeover/CameraControls/*p
  "$ENGINE" $ROOT/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nosound \
    -RenderOffscreen -windowed -ResX=$W -ResY=$H -DisablePlugins=Fab,BpGeneratorUltimate \
    -ExecCmds="r.TextureStreaming 0, r.SetRes ${W}x${H}w" -ForceRes -CameraControlsTakeoverValidate -abslog=$LOG > /dev/null 2>&1
  OUT=$ROOT/Saved/InfoTakeover/PageLamps/$RES
  rm -rf "$OUT"; mkdir -p "$OUT"
  cp "$ROOT"/Saved/InfoTakeover/CameraControls/*p/*.png "$OUT"/ 2>/dev/null
  echo "== $RES"
  grep -E "CAMERA_TAKEOVER (page lamps|PASS|FAIL|RESULT)|CAMERA_TAKEOVER.*(fail|FAIL)" $LOG | cut -c1-330
done
