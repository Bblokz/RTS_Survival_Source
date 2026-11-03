#!/bin/bash
# Usage: run_ragdoll_tests.sh [project dir] [test filter]  -> runs the ragdoll automation tests headless under -nullrhi.
# Project dir defaults to this checkout; pass a linked copy with its own Binaries when the editor DLLs are locked.
# bAddPacks is forced off: a project folder without Saved/ copies StarterContent into Content, through any link.
PROJECT_DIR=${1:-D:/UE5Projects/RTS_Survival}
FILTER=${2:-RTS.Ragdoll}
LOG=D:/UE5Projects/RTS_Survival/Saved/Logs/RagdollTests.log
REPORT=D:/UE5Projects/RTS_Survival/Saved/RagdollTests
rm -rf "$REPORT"; mkdir -p "$REPORT"
MSYS_NO_PATHCONV=1 timeout 900 "/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" \
  "$PROJECT_DIR/RTS_Survival.uproject" -nullrhi -unattended -nop4 -nosplash -nosound -DisablePlugins=Fab \
  "-ini:Game:[StartupActions]:bAddPacks=False" \
  "-ExecCmds=Automation RunTests $FILTER" "-TestExit=Automation Test Queue Empty" "-ReportExportPath=$REPORT" \
  "-abslog=$LOG" > /dev/null 2>&1
echo "exit $?"
grep -a -E "Test Completed|Error:.*(Ragdoll|Automation)|Expected|LogAutomationController: (Error|Warning)|tests? (passed|failed)|RTS_RGC request" "$LOG" | sed 's/^\[[^]]*\]\[[^]]*\]//' | head -200
