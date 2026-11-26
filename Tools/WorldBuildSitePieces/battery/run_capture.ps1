# Waits until the graphics card has room, then takes the battery piece pictures in a throwaway DebugGame editor
# on the world campaign level (own save slot; the level is not saved). Pictures: Saved\WorldBuildSitePieces\battery_*.png
param([string]$Map = "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign")
$out = "D:\UE5Projects\RTS_Survival\Saved\WorldBuildSitePieces"
$log = "$out\battery\capture.log"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
$deadline = (Get-Date).AddMinutes(90)
# Other sessions render offscreen too: start only after the card is below 6500 MiB AND no offscreen editor has run
# for three polls in a row (another worker runs two editors back to back).
$quiet = 0
while ($true) {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    $offscreen = @(Get-CimInstance Win32_Process -Filter "Name like 'UnrealEditor%'" |
        Where-Object { $_.CommandLine -match 'RenderOffscreen' }).Count
    if ($gpu -le 6500 -and $offscreen -eq 0) { $quiet++ } else { $quiet = 0 }
    if ($quiet -ge 3) { break }
    if ((Get-Date) -gt $deadline) { "GPU still at $gpu MiB after 90 minutes, not starting"; exit 2 }
    Start-Sleep -Seconds 20
}
"gpu used before: $gpu MiB at $(Get-Date -Format HH:mm:ss)"
Remove-Item $log -ErrorAction SilentlyContinue
Remove-Item "$out\battery_*.png" -ErrorAction SilentlyContinue
$arguments = @('"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"',
    ('-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=' + $Map),
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldBuildSitePieces/battery/capture_battery.py"',
    '-RTSWorldCampaignSaveSlot=BatteryPieceCapture',
    '-unattended', '-nop4', '-nosound', '-nosplash', '-RenderOffscreen', '-NoLiveCoding',
    '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
$stop = (Get-Date).AddSeconds(560)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Select-String -Path $log -Pattern "BATTERYCAP|Fatal|Assertion|Traceback|Error: " | Select-Object -First 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(220, $_.Line.Length)) }
Get-ChildItem $out -Filter battery_*.png | Select-Object Name, Length
