#!/usr/bin/env bash
# Runs UFieldKitGunnerTestSubsystem (-FieldKitGunnerValidate) on RTS_ToCopySmall: spawns every MG 42 hatch gunner kit
# tank and variant plus the Pz III J / Pz IV F1 controls, researches the kit and checks each gunner.
# Usage: run_field_kit_test.sh [capture]   capture = render offscreen and save 3 pictures per tank to
#        Saved/FieldKits/Capture (check nvidia-smi first: a render run needs ~2 GB of GPU memory).
set -u
MAP="${MAP:-/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall}"
ENGINE="D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe"
LOG="D:/UE5Projects/RTS_Survival/Saved/FieldKits/FieldKitTest.log"
if [ "${1:-}" = "capture" ]; then
  RENDER=(-RenderOffscreen -windowed -ResX=1280 -ResY=720 "-ExecCmds=r.TextureStreaming 0"
          -FKCapture=D:/UE5Projects/RTS_Survival/Saved/FieldKits/Capture)
else
  RENDER=(-nullrhi)
fi
MSYS_NO_PATHCONV=1 "$ENGINE" "D:/UE5Projects/RTS_Survival/RTS_Survival.uproject" "$MAP" \
  -game -unattended -nosound -DisablePlugins=Fab -benchmark -fps=30 "${RENDER[@]}" \
  -FieldKitGunnerValidate -abslog="$LOG"
echo "exit=$? log=$LOG"
grep -E "RTS_FIELDKIT_TEST (CHECK FAIL|RESULT|research|START)|FIELDKIT|Field kit" "$LOG"
