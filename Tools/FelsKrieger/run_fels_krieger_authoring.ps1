# Runs author_fels_krieger_drop.py (GA_GerFelsKriegerDrop template) headless on the DebugGame build.
# Hold Saved/AgentLocks/debuggame_build.lockdir AND da_globalabilities.lockdir while this runs.
param([string]$Log = "$PSScriptRoot\..\..\..\Saved\FelsKriegerTest\authoring.log")
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "author_fels_krieger_drop.py").Replace([char]92, [char]47)
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-AllowCommandletRendering",
    "-RenderOffscreen", "-NoLiveCoding", "-DisablePlugins=Fab", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
$Process.WaitForExit(1200000) | Out-Null
Select-String -Path $Log -Pattern "\[FelsKriegerDrop\]" | ForEach-Object { $_.Line }
