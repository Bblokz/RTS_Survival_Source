# Runs run_ju87g_headless.py (Ju 87 G Blueprint + GA_GerJu87GTankBusters template) on the DebugGame build.
# Hold Saved/AgentLocks/debuggame_build.lockdir AND da_globalabilities.lockdir while this runs.
param([string]$Log = "$PSScriptRoot\..\..\..\Saved\Ju87GTest\authoring.log")
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "run_ju87g_headless.py").Replace([char]92, [char]47)
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-AllowCommandletRendering",
    "-RenderOffscreen", "-NoLiveCoding", "-DisablePlugins=Fab", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
$Process.WaitForExit(1200000) | Out-Null
Select-String -Path $Log -Pattern "\[Ju87G\]|\[Ju87GTankBusters\]" | ForEach-Object { $_.Line }
