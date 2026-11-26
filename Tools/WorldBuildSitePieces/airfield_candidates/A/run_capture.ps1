# Waits until the graphics card has room and no other offscreen render runs, then takes the airfield candidate A
# pictures in a throwaway DebugGame editor on the world campaign level (own save slot, level not saved).
# Pictures: Saved/WorldBuildSitePieces/airfield_candidate_A_*.png   Log: Saved/WorldBuildSitePieces/airfield_candidate_A/capture.log
param([string]$Map = "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign", [int]$WaitMinutes = 120)
$out = "D:\UE5Projects\RTS_Survival\Saved\WorldBuildSitePieces"
$log = "$out\airfield_candidate_A\capture.log"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
New-Item -ItemType Directory -Force "$out\airfield_candidate_A" | Out-Null

function Get-OtherRenders {
    # Offscreen / -game UE processes (renders, captures, Shorts). The owner's normal editor does not count.
    Get-CimInstance Win32_Process -Filter "Name like 'UnrealEditor%'" | Where-Object {
        $_.CommandLine -match '(?i)-RenderOffscreen|\s-game(\s|$)|MovieRenderPipeline|-MoviePipeline'
    }
}

$deadline = (Get-Date).AddMinutes($WaitMinutes)
while ($true) {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    $others = @(Get-OtherRenders)
    if ($gpu -le 6500 -and $others.Count -eq 0) {
        # Re-check after a short pause so two waiting agents do not start in the same second.
        Start-Sleep -Seconds (5 + (Get-Random -Maximum 20))
        $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
        $others = @(Get-OtherRenders)
        if ($gpu -le 6500 -and $others.Count -eq 0) { break }
    }
    if ((Get-Date) -gt $deadline) { "still busy after $WaitMinutes minutes (GPU $gpu MiB, other renders $($others.Count)), not starting"; exit 2 }
    Start-Sleep -Seconds 30
}
"gpu used before: $gpu MiB at $(Get-Date -Format HH:mm:ss)"
Remove-Item $log -ErrorAction SilentlyContinue
Remove-Item "$out\airfield_candidate_A_*.png" -ErrorAction SilentlyContinue
$arguments = @('"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"',
    ('-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=' + $Map),
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldBuildSitePieces/airfield_candidates/A/capture_airfield_a.py"',
    '-RTSWorldCampaignSaveSlot=AirfieldCandidateACapture',
    '-unattended', '-nop4', '-nosound', '-nosplash', '-RenderOffscreen', '-NoLiveCoding',
    '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
$stop = (Get-Date).AddSeconds(720)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Select-String -Path $log -Pattern "AIRFIELD_A|Fatal|Assertion|Traceback|AFA_Paint|M_AirfieldA_Paint" | Select-Object -First 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
Get-ChildItem $out -Filter airfield_candidate_A_*.png | Select-Object Name, Length
