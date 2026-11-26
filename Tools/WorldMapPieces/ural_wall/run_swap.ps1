# Throwaway DebugGame editor on the world campaign level: swaps the wall meshes (swap_ural_wall.py) and takes pictures.
#   -Save : also saves RTS_WorldCampaign.umap. Without it nothing is saved (preview).   -Old : pictures of the old wall.
# Waits until no other offscreen render runs and the graphics card is below 6.5 GB.
param([switch]$Save, [switch]$Old, [string]$Root = "D:\UE5Projects\RTS_Survival_DeckTest")
$out = "$Root\Saved\WorldMapWall"
$log = "$out\swap.log"
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
$env:URALWALL_SAVE = if ($Save) { "1" } else { "0" }
$env:URALWALL_SWAP = if ($Old) { "0" } else { "1" }
Remove-Item $log -ErrorAction SilentlyContinue
$arguments = @("`"$Root\RTS_Survival.uproject`"",
    '-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign',
    '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False',
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldMapPieces/ural_wall/swap_ural_wall.py"',
    '-RTSWorldCampaignSaveSlot=UralWallSwap', '-unattended', '-nop4', '-nosound', '-nosplash', '-RenderOffscreen',
    '-NoLiveCoding', '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
$stop = (Get-Date).AddSeconds(720)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Select-String -Path $log -Pattern "URALWALL|Fatal|Assertion|Traceback|Failed to compile" | Select-Object -First 40 |
    ForEach-Object { $_.Line.Substring(0, [Math]::Min(240, $_.Line.Length)) }
Get-ChildItem $out -Filter wall_*.png | Select-Object Name, Length, LastWriteTime
"END $(Get-Date -Format HH:mm:ss)"
