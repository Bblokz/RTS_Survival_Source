# -game run of the world campaign on its own save slot with probe_wall_clearance.py; prints Saved/WorldMapWall/clearance.txt.
# Waits until no other offscreen render runs and the graphics card is below 6.5 GB.
param([string]$Root = "D:\UE5Projects\RTS_Survival_DeckTest")
$out = "$Root\Saved\WorldMapWall"
$log = "$out\probe.log"
$slot = "UralWallProbe"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
New-Item -ItemType Directory -Force $out | Out-Null
$deadline = (Get-Date).AddHours(3)
while ((Get-Date) -lt $deadline) {
    $gpu = [int]((nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) | Select-Object -First 1)
    $others = @(Get-CimInstance Win32_Process -Filter "Name like 'UnrealEditor%'" |
        Where-Object { $_.CommandLine -match '(?i)-RenderOffscreen|\s-game(\s|$)|MoviePipeline' })
    if ($gpu -le 6500 -and $others.Count -eq 0) { break }
    Start-Sleep -Seconds 30
}
"START $(Get-Date -Format HH:mm:ss) gpu=$gpu"
Remove-Item $log, "$out\clearance.txt" -ErrorAction SilentlyContinue
$arguments = @("`"$Root\RTS_Survival.uproject`"", "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign", "-game",
    "-unattended", "-nop4", "-nosound", "-RenderOffscreen", "-windowed", "-ResX=1280", "-ResY=720", "-NoLiveCoding",
    "-DisablePlugins=Fab,BpGeneratorUltimate", "-ini:Game:[StartupActions]:bAddPacks=False",
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldMapPieces/ural_wall/probe_wall_clearance.py"',
    "-RTSWorldCampaignSaveSlot=$slot", "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden
if (-not $process.WaitForExit(300000)) { "TIMEOUT"; $process.Kill() }
Remove-Item "$Root\Saved\SaveGames\$slot.sav", "$Root\Saved\SaveGames\backup\$slot.sav" -ErrorAction SilentlyContinue
if (Test-Path "$out\clearance.txt") { Get-Content "$out\clearance.txt" } else {
    Select-String -Path $log -Pattern "URALWALL|Traceback|Error: " | Select-Object -Last 20 | ForEach-Object { $_.Line }
}
"END $(Get-Date -Format HH:mm:ss)"
