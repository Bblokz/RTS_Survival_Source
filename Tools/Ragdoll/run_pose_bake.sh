#!/bin/bash
# Usage: run_pose_bake.sh <label> [project dir] [extra args, e.g. -RagdollPoseSeed=2]
#  -> candidate corpse poses as SM_CorpsePose_* in /Game/.../SquadUnits/CorpsePoses/Candidates, pictures in
#     Saved/RagdollDemo/<label>, log Saved/Logs/RagdollDemo_<label>.log
# Project dir defaults to this checkout; pass a linked copy with its own Binaries when the editor DLLs are locked.
LABEL=$1; shift
PROJECT_DIR=${1:-D:/UE5Projects/RTS_Survival}; [ $# -gt 0 ] && shift
cd /d/UE5Projects/RTS_Survival || exit 1
OUT=D:/UE5Projects/RTS_Survival/Saved/RagdollDemo/$LABEL
rm -rf "Saved/RagdollDemo/$LABEL"; mkdir -p "Saved/RagdollDemo/$LABEL"
MSYS_NO_PATHCONV=1 timeout 700 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "$PROJECT_DIR/RTS_Survival.uproject" /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nop4 \
  -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab -benchmark -fps=30 \
  "-ini:Game:[StartupActions]:bAddPacks=False" "-ExecCmds=r.TextureStreaming 0" \
  -RagdollDemo -RagdollPoseBake "-RagdollDemoDir=$OUT" "-RagdollDemoAt=16796,-15808" "$@" \
  "-abslog=D:/UE5Projects/RTS_Survival/Saved/Logs/RagdollDemo_$LABEL.log" > /dev/null 2>&1
echo "exit $?"
grep -a -E "RTS_RGP|RTS_RGD RESULT|Error:.*(Ragdoll|Pose|StaticMesh|SavePackage)" "Saved/Logs/RagdollDemo_$LABEL.log" | sed 's/^.*Display: //' | head -100
echo "pictures: $(ls Saved/RagdollDemo/$LABEL | wc -l)"
