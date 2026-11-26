# Waits until the graphics card has room, then takes the watch post pictures in a throwaway DebugGame editor on the
# world campaign level (own save slot, level not saved). Pictures: Saved/WorldBuildSitePieces/watch_post_*.png
param([string]$Map = "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign")
$out = "D:\UE5Projects\RTS_Survival\Saved\WorldBuildSitePieces"
$log = "$out\capture.log"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
New-Item -ItemType Directory -Force $out | Out-Null
$deadline = (Get-Date).AddMinutes(25)
while ($true) {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    if ($gpu -le 6500) { break }
    if ((Get-Date) -gt $deadline) { "GPU still at $gpu MiB after 25 minutes, not starting"; exit 2 }
    Start-Sleep -Seconds 20
}
"gpu used before: $gpu MiB"
Remove-Item $log -ErrorAction SilentlyContinue
Remove-Item "$out\watch_post_*.png" -ErrorAction SilentlyContinue
$arguments = @('"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"',
    ('-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=' + $Map),
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldBuildSitePieces/watch_post/capture_watch_post.py"',
    '-RTSWorldCampaignSaveSlot=WatchPostPieceCapture',
    '-unattended', '-nop4', '-nosound', '-nosplash', '-RenderOffscreen', '-NoLiveCoding',
    '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
$stop = (Get-Date).AddSeconds(600)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Select-String -Path $log -Pattern "WATCHPOST|Fatal|Assertion|Traceback|WP_WatchPostCamo|M_WatchPost_Paint" | Select-Object -First 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
Get-ChildItem $out -Filter watch_post_*.png | Select-Object Name, Length
