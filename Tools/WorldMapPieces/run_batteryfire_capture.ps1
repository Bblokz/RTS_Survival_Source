# Plays one battery fire mission on the campaign map in a throwaway save slot and takes five pictures.
# Waits until the graphics card has room. Pictures: Saved\WorldCampaignUICapture\batteryfire_*.png
# -Set "Name=Value;Name=Value" (one string, split on ;): overrides of UWorldBatteryFireSettings for this run only.
# -Tag x: also copies the pictures to Saved\WorldCampaignUICapture\<x>\ so several runs can be compared.
param([string]$Set = "", [string]$Tag = "")
$root = "D:\UE5Projects\RTS_Survival"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$slot = "BatteryFireCapture"
$log = "$root\Saved\WorldCampaignContent\batteryfire_capture.log"
$pictures = "$root\Saved\WorldCampaignUICapture"
$deadline = (Get-Date).AddMinutes(30)
while ($true) {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    $running = @(Get-Process -Name "UnrealEditor-Win64-DebugGame-Cmd" -ErrorAction SilentlyContinue |
        Where-Object { $_.MainWindowTitle -ne $null -and $_.WorkingSet64 -gt 6GB }).Count
    if ($gpu -le 6500) { break }
    if ((Get-Date) -gt $deadline) { "GPU still at $gpu MiB after 30 minutes, not starting"; exit 2 }
    Start-Sleep -Seconds 20
}
"gpu used before: $gpu MiB"
Remove-Item $log -ErrorAction SilentlyContinue
Remove-Item "$pictures\batteryfire_*.png" -ErrorAction SilentlyContinue
$arguments = @("`"$root\RTS_Survival.uproject`"", '/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign', '-game',
    '-RenderOffscreen', '-windowed', '-ResX=1920', '-ResY=1080', '-unattended', '-nop4', '-nosound', '-nosplash',
    '-NoLiveCoding', '-DisablePlugins=Fab,BpGeneratorUltimate', '-WorldCampaignBatteryFireCapture',
    "-RTSWorldCampaignSaveSlot=$slot", "-abslog=$log")
foreach ($pair in ($Set -split ";" | Where-Object { $_ -ne "" })) { $arguments += "-ini:Game:[/Script/RTS_Survival.WorldBatteryFireSettings]:$pair" }
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden
$stop = (Get-Date).AddSeconds(420)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Start-Sleep -Seconds 2
Remove-Item "$root\Saved\SaveGames\$slot.sav", "$root\Saved\SaveGames\backup\$slot.sav" -ErrorAction SilentlyContinue
Select-String -Path $log -Pattern "BATTERYFIRE|Fatal|Assertion" |
    Where-Object { $_.Line -notmatch "picture|Tracing|Command Line|commandline" } | Select-Object -First 10 |
    ForEach-Object { $_.Line.Substring(0, [Math]::Min(230, $_.Line.Length)) }
if ($Tag -ne "") {
    New-Item -ItemType Directory -Force "$pictures\$Tag" | Out-Null
    Copy-Item "$pictures\batteryfire_*.png" "$pictures\$Tag"
}
Get-ChildItem $pictures -Filter "batteryfire_*.png" | ForEach-Object { "$($_.Name) $($_.Length)" }
