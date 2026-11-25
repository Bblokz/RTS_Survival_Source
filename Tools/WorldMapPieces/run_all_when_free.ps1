# Waits until no other run holds the piece assets and the graphics card has room, then: rebuild the pieces
# (headless), take the line-up pictures, and run the world map UI capture given as the first argument.
param([string]$UiCaptureScript = "")
$root = "D:\UE5Projects\RTS_Survival"
$probe = "$root\Content\RTS_Survival\WorldCampaign\MapObjects\Missions\Story\OreMountains\BP_Mission_OreMountains.uasset"
$deadline = (Get-Date).AddMinutes(45)
while ($true) {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    $locked = $false
    try { $stream = [System.IO.File]::Open($probe, 'Open', 'ReadWrite', 'None'); $stream.Close() } catch { $locked = $true }
    if (-not $locked -and $gpu -le 6500) { break }
    if ((Get-Date) -gt $deadline) { "still busy after 45 minutes (gpu $gpu MiB, locked $locked), not starting"; exit 2 }
    Start-Sleep -Seconds 20
}
"free at $(Get-Date -Format HH:mm:ss): gpu $gpu MiB"
& "C:\Program Files\Git\bin\sh.exe" "$root/Source/Tools/WorldMapPieces/run_build.sh" build_pieces.py "OreMountains,Berlin,OderWarthe,Pripyat,FrozenLake" |
    Select-String -Pattern "Traceback|blueprint|FAILED|WRONG|NO MATERIAL|exit=" | ForEach-Object { $_.Line.Substring(0, [Math]::Min(200, $_.Line.Length)) }
"--- lineup"
& powershell -ExecutionPolicy Bypass -File "$root\Source\Tools\WorldMapPieces\run_lineup.ps1" |
    Select-String -Pattern "gpu|killed|png|Traceback|failed|not starting"
if ($UiCaptureScript -ne "") {
    "--- ui capture"
    & powershell -ExecutionPolicy Bypass -File $UiCaptureScript
}
