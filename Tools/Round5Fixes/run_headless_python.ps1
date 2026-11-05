# Runs one Round5Fixes python script headless on the DebugGame build and prints its [Round5...] lines.
# Hold Saved/AgentLocks/debuggame_build.lockdir (and da_globalabilities.lockdir when the script saves the DA).
param([Parameter(Mandatory=$true)][string]$ScriptName, [string]$Log = "")
if ($Log -eq "") { $Log = "D:\UE5Projects\RTS_Survival\Saved\Round5FixesTest\$([IO.Path]::GetFileNameWithoutExtension($ScriptName))_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot $ScriptName).Replace([char]92, [char]47)
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-AllowCommandletRendering",
    "-RenderOffscreen", "-NoLiveCoding", "-DisablePlugins=Fab", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
$Process.WaitForExit(1800000) | Out-Null
Write-Output "LOG $Log EXIT $($Process.ExitCode)"
Select-String -Path $Log -Pattern "\[Round5" | ForEach-Object { $_.Line }
