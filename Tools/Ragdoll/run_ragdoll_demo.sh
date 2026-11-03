#!/bin/bash
# Usage: run_ragdoll_demo.sh <label> [extra args, e.g. -RagdollDemoSquad=Squad_Rus_Mosin]  -> frames in Saved/RagdollDemo/<label>, log Saved/Logs/RagdollDemo_<label>.log
# PROJECT_DIR=<linked copy> runs another project folder with its own Binaries when the editor DLLs are locked.
LABEL=$1; shift
PROJECT_DIR=${PROJECT_DIR:-D:/UE5Projects/RTS_Survival}
cd /d/UE5Projects/RTS_Survival || exit 1
OUT=D:/UE5Projects/RTS_Survival/Saved/RagdollDemo/$LABEL
rm -rf "Saved/RagdollDemo/$LABEL"; mkdir -p "Saved/RagdollDemo/$LABEL"
MSYS_NO_PATHCONV=1 timeout 700 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "$PROJECT_DIR/RTS_Survival.uproject" /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nop4 \
  "-ini:Game:[StartupActions]:bAddPacks=False" \
  -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab -benchmark -fps=30 "-ExecCmds=r.TextureStreaming 0" \
  -RagdollDemo "-RagdollDemoDir=$OUT" "-RagdollDemoAt=16796,-15808" "$@" \
  "-abslog=D:/UE5Projects/RTS_Survival/Saved/Logs/RagdollDemo_$LABEL.log" > /dev/null 2>&1
echo "exit $?"
grep -a -E "RTS_RGD|RTS_RGC|RTS_RGB|Error:.*(Ragdoll|SquadUnit)" "Saved/Logs/RagdollDemo_$LABEL.log" | sed 's/^.*Display: //' | head -300
echo "frames: $(ls Saved/RagdollDemo/$LABEL | wc -l)"
