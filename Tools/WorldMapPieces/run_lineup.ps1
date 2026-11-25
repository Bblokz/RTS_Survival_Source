# Waits until the graphics card has room, then takes the line-up pictures in a throwaway DebugGame editor.
param([string]$Map = "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign", [string]$TestMap = "", [string]$Variants = "")
$env:WORLDPIECES_TESTMAP = $TestMap
$env:WORLDPIECES_VARIANTS = $Variants
$out = "D:\UE5Projects\RTS_Survival\Saved\WorldMapPieces"
$log = "$out\lineup.log"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
$deadline = (Get-Date).AddMinutes(25)
while ($true) {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    if ($gpu -le 6500) { break }
    if ((Get-Date) -gt $deadline) { "GPU still at $gpu MiB after 25 minutes, not starting"; exit 2 }
    Start-Sleep -Seconds 20
}
"gpu used before: $gpu MiB"
Remove-Item $log -ErrorAction SilentlyContinue
if ($Variants -eq "1") { Remove-Item "$out\variants_*.png" -ErrorAction SilentlyContinue }
else { Remove-Item "$out\lineup_*.png", "$out\piece_*.png", "$out\reference_*.png" -ErrorAction SilentlyContinue }
$arguments = @('"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"',
    ('-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=' + $Map),
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldMapPieces/lineup_capture.py"',
    '-unattended', '-nop4', '-nosound', '-nosplash', '-RenderOffscreen', '-NoLiveCoding',
    '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
$stop = (Get-Date).AddSeconds(560)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Select-String -Path $log -Pattern "LINEUP|Fatal|Assertion|Traceback|Error: " | Select-Object -First 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(220, $_.Line.Length)) }
Get-ChildItem $out -Filter *.png | Select-Object Name, Length
