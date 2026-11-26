# Waits until the graphics card has room AND no other offscreen Unreal render runs, then takes the candidate B pictures
# in a throwaway DebugGame editor on the world campaign level (own save slot, level not saved).
# Pictures: Saved/WorldBuildSitePieces/airfield_candidate_B_*.png   Log: Saved/WorldBuildSitePieces/airfield_B/capture.log
param([string]$Map = "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign", [int]$Minutes = 150)
$out = "D:\UE5Projects\RTS_Survival\Saved\WorldBuildSitePieces"
$log = "$out\airfield_B\capture.log"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
New-Item -ItemType Directory -Force "$out\airfield_B" | Out-Null

function Test-GpuFree {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    $renders = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'UnrealEditor%'" |
        Where-Object { $_.CommandLine -match 'RenderOffscreen' })
    return @{ Free = ($gpu -le 6500 -and $renders.Count -eq 0); Gpu = $gpu; Renders = $renders.Count }
}

$deadline = (Get-Date).AddMinutes($Minutes)
while ($true) {
    $state = Test-GpuFree
    if ($state.Free) {
        # Other agents wait on the same condition: back off a random moment and look again before starting.
        Start-Sleep -Seconds (Get-Random -Minimum 15 -Maximum 75)
        $state = Test-GpuFree
        if ($state.Free) { break }
    }
    if ((Get-Date) -gt $deadline) { "GPU busy ($($state.Gpu) MiB, $($state.Renders) offscreen renders) after $Minutes minutes, not starting"; exit 2 }
    Start-Sleep -Seconds 20
}
"$(Get-Date -Format HH:mm:ss) gpu used before: $($state.Gpu) MiB"
Remove-Item $log -ErrorAction SilentlyContinue
Remove-Item "$out\airfield_candidate_B_*.png" -ErrorAction SilentlyContinue
$arguments = @('"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"',
    ('-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=' + $Map),
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldBuildSitePieces/airfield_candidates/B/capture_airfield_b.py"',
    '-RTSWorldCampaignSaveSlot=AirfieldCandidateBCapture',
    '-unattended', '-nop4', '-nosound', '-nosplash', '-RenderOffscreen', '-NoLiveCoding',
    '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
$stop = (Get-Date).AddSeconds(660)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Select-String -Path $log -Pattern "AIRFIELDBCAP|Fatal|Assertion|Traceback|AFB_Paint|M_AirfieldB_Paint" | Select-Object -First 50 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
Get-ChildItem $out -Filter "airfield_candidate_B_*.png" | Select-Object Name, Length
