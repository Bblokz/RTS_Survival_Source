# Runs round3a_render_parts.py in its own DebugGame editor process on Template_Default (never the shared editor).
# Hold Saved/AgentLocks/debuggame_build.lockdir while this runs. -Jobs limits the run to a comma-separated subset.
param([string]$Jobs = "", [string]$Dump = "", [string]$Log = "D:\UE5Projects\RTS_Survival\Saved\AbilityIconsA\render.log")
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "round3a_render_parts.py").Replace([char]92, [char]47)
$Done = "D:\UE5Projects\RTS_Survival\Saved\GlobalAbilityIcons\renders\round3a_done.txt"
if (Test-Path $Done) { Remove-Item $Done }
$env:ICONSA_JOBS = $Jobs
$env:ICONSA_DUMP = $Dump
$Arguments = @($Project, "/Engine/Maps/Templates/Template_Default", "-NoLiveCoding", "-unattended", "-nosplash",
    "-DisablePlugins=Fab", "-ExecCmds=`"py $Script`"", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
$Finished = $Process.WaitForExit(900000)
if (-not $Finished) { "TIMEOUT pid $($Process.Id) still running" }
if (Test-Path $Done) { Get-Content $Done } else { "NO DONE FILE" }
Select-String -Path $Log -Pattern "\[IconsA\]|RENDERED|Error: " | Select-Object -First 80 | ForEach-Object { $_.Line }
