# Renders the DF station icon parts / previews headless (render_df_parts.py) in the Development commandlet.
# One GPU job at a time: check nvidia-smi and Get-Process UnrealEditor* first.
param([string]$Jobs = "all", [int]$Passes = 2)
$Work = "D:\UE5Projects\RTS_Survival\Saved\OderWarthe\stations\icons"
New-Item -ItemType Directory -Force "$Work\renders" | Out-Null
$JobsJson = if ($Jobs -eq "all") { '"all"' } else { '["' + (($Jobs -split ",") -join '","') + '"]' }
Set-Content -Path "$Work\run_config.json" -Value "{`"jobs`": $JobsJson, `"passes`": $Passes}" -Encoding ascii
$Log = "$Work\headless.log"
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "render_df_parts.py").Replace([char]92, [char]47)
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-AllowCommandletRendering",
    "-RenderOffscreen", "-NoLiveCoding", "-DisablePlugins=Fab", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -WindowStyle Hidden -PassThru
$Finished = $Process.WaitForExit(1800000)
if (-not $Finished) { "TIMEOUT pid $($Process.Id)" }
"EXIT $($Process.ExitCode)"
Select-String -Path $Log -Pattern "\[DFIcons\]|Error: " | Select-Object -Last 40 | ForEach-Object { $_.Line }
