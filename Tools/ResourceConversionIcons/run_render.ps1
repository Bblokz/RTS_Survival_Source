# Runs render_vehicle_parts.py in a Development commandlet (no DebugGame DLL lock, nothing in the shared editor).
# Check Get-Process UnrealEditor* for a running Shorts render first (one heavy GPU job at a time).
param([string]$Log = "")
if ($Log -eq "") { $Log = "$PSScriptRoot\..\..\..\Saved\GlobalAbilityIcons\conversion_render.log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "render_vehicle_parts.py").Replace([char]92, [char]47)
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-AllowCommandletRendering",
    "-RenderOffscreen", "-NoLiveCoding", "-DisablePlugins=Fab", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -WindowStyle Hidden -PassThru
$Finished = $Process.WaitForExit(1800000)
if (-not $Finished) { "TIMEOUT pid $($Process.Id)" }
"EXIT $($Process.ExitCode)"
Select-String -Path $Log -Pattern "\[ConvIcons\]|Error: " | Select-Object -Last 40 | ForEach-Object { $_.Line }
