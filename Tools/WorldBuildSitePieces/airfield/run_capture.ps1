# Waits until the graphics card has room, then takes the airfield piece pictures in a throwaway DebugGame editor.
# -Mode neutral: unit test map, plain light.  -Mode campaign: world campaign level (with a throwaway save slot).
param([string]$Mode = "neutral")
$out = "D:\UE5Projects\RTS_Survival\Saved\WorldBuildSitePieces"
$log = "$out\airfield_capture_$Mode.log"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
$env:AIRFIELD_CAPTURE_MODE = $Mode
$map = "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall"
if ($Mode -eq "campaign") { $map = "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign" }
$deadline = (Get-Date).AddMinutes(25)
while ($true) {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    if ($gpu -le 6500) { break }
    if ((Get-Date) -gt $deadline) { "GPU still at $gpu MiB after 25 minutes, not starting"; exit 2 }
    Start-Sleep -Seconds 20
}
"gpu used before: $gpu MiB"
Remove-Item $log -ErrorAction SilentlyContinue
Remove-Item "$out\airfield_$($Mode)_*.png" -ErrorAction SilentlyContinue
$arguments = @('"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"',
    ('-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=' + $map),
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldBuildSitePieces/airfield/capture_airfield_piece.py"',
    '-RTSWorldCampaignSaveSlot=AirfieldPieceCapture',
    '-unattended', '-nop4', '-nosound', '-nosplash', '-RenderOffscreen', '-NoLiveCoding',
    '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
$stop = (Get-Date).AddSeconds(620)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Select-String -Path $log -Pattern "AIRFIELDCAP|Fatal|Assertion|Traceback" | Select-Object -First 40 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(220, $_.Line.Length)) }
Get-ChildItem $out -Filter "airfield_$($Mode)_*.png" | Select-Object Name, Length
