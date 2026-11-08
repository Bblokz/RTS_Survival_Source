# Runs sky_plates_render.py as a commandlet (sky plates).
#   -Build Development : render / read-only dump runs. Uses UnrealEditor-Cmd.exe (the Development DLL the shared editor
#                        already pins), so it does not lock the DebugGame DLL; no build lock needed. Saves nothing but
#                        PNGs / texture imports (never the DA).
#   -Build DebugGame   : the DA icon assign. Hold Saved/AgentLocks/debuggame_build.lockdir AND da_globalabilities.lockdir.
# Check Get-Process UnrealEditor* for a running Shorts render first (one heavy GPU job at a time).
param([string]$Build = "Development", [string]$Log = "")
if ($Log -eq "") { $Log = "$PSScriptRoot\..\..\..\Saved\GlobalAbilityIcons\sky\headless.log" }
$Bin = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64"
$Engine = if ($Build -eq "DebugGame") { "$Bin\UnrealEditor-Win64-DebugGame-Cmd.exe" } else { "$Bin\UnrealEditor-Cmd.exe" }
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "sky_plates_render.py").Replace([char]92, [char]47)
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-AllowCommandletRendering",
    "-RenderOffscreen", "-NoLiveCoding", "-DisablePlugins=Fab", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -WindowStyle Hidden -PassThru
$Finished = $Process.WaitForExit(1800000)
if (-not $Finished) { "TIMEOUT pid $($Process.Id)" }
"EXIT $($Process.ExitCode)"
Select-String -Path $Log -Pattern "\[SkyPlates\]|RENDERED|Error: " | Select-Object -Last 80 | ForEach-Object { $_.Line }
