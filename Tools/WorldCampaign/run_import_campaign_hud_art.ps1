# Runs import_campaign_hud_art.py as a commandlet. Uses the Development UnrealEditor-Cmd.exe: it only imports
# textures, so it needs no C++ of the campaign and does not lock the DebugGame DLL.
param([string]$Log = "")
if ($Log -eq "") { $Log = "$PSScriptRoot\..\..\..\Saved\WorldCampaignContent\import_hud_art.log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "import_campaign_hud_art.py").Replace([char]92, [char]47)
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-NoLiveCoding", "-DisablePlugins=Fab",
    "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -WindowStyle Hidden -PassThru
$Finished = $Process.WaitForExit(1200000)
if (-not $Finished) { "TIMEOUT pid $($Process.Id)" }
"EXIT $($Process.ExitCode)"
Select-String -Path $Log -Pattern "\[CampaignArt\]|Error: " | Select-Object -Last 40 | ForEach-Object { $_.Line }
