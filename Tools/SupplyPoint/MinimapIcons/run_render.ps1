# Renders the supply icon parts headless (render_supply_parts.py) in the Development UnrealEditor-Cmd commandlet:
# no DebugGame lock, nothing in the shared editor, nothing saved but PNGs under Saved/SupplyPointMinimapIcons.
# Check Get-Process UnrealEditor* for another heavy render first (one GPU job at a time).
param([string]$Jobs = "all", [int]$Passes = 2, [string]$Prefix = "", [switch]$Discover)
$Work = "D:\UE5Projects\RTS_Survival\Saved\SupplyPointMinimapIcons"
New-Item -ItemType Directory -Force "$Work\renders" | Out-Null
$JobsJson = if ($Jobs -eq "all") { '"all"' } else { '["' + (($Jobs -split ",") -join '","') + '"]' }
if ($Prefix -ne "") { $JobsJson = '"' + $Prefix + '"' }
$DiscoverJson = if ($Discover) { "true" } else { "false" }
Set-Content -Path "$Work\run_config.json" -Value "{`"jobs`": $JobsJson, `"passes`": $Passes, `"discover`": $DiscoverJson}" -Encoding ascii
$Log = "$Work\headless.log"
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "render_supply_parts.py").Replace([char]92, [char]47)
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-AllowCommandletRendering",
    "-RenderOffscreen", "-NoLiveCoding", "-DisablePlugins=Fab", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -WindowStyle Hidden -PassThru
$Finished = $Process.WaitForExit(1800000)
if (-not $Finished) { "TIMEOUT pid $($Process.Id)" }
"EXIT $($Process.ExitCode)"
Select-String -Path $Log -Pattern "\[SupplyIcons\]|RENDERED|Error: " | Select-Object -Last 80 | ForEach-Object { $_.Line }
