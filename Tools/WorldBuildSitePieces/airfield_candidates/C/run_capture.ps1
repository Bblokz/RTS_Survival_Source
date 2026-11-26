# Waits until the graphics card has room and no other offscreen Unreal render runs, then takes the candidate C pictures
# in a throwaway DebugGame editor on the world campaign level (own save slot, level not saved).
# Pictures: Saved/WorldBuildSitePieces/airfield_candidate_C_*.png   Log: Saved/WorldBuildSitePieces/airfield_C/capture.log
param([string]$Map = "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign", [int]$WaitMinutes = 120)
$out = "D:\UE5Projects\RTS_Survival\Saved\WorldBuildSitePieces"
$log = "$out\airfield_C\capture.log"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
New-Item -ItemType Directory -Force "$out\airfield_C" | Out-Null

function Test-GpuFree {
    $gpu = (& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) -as [int]
    $renders = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'UnrealEditor%'" |
        Where-Object { $_.CommandLine -match '(?i)-RenderOffscreen' })
    return @{ Free = ($gpu -le 6500 -and $renders.Count -eq 0); Gpu = $gpu; Renders = $renders.Count }
}

$deadline = (Get-Date).AddMinutes($WaitMinutes)
while ($true) {
    $state = Test-GpuFree
    if ($state.Free) {
        # Settle a random moment and look again, so two waiting sessions do not start together.
        Start-Sleep -Seconds (Get-Random -Minimum 10 -Maximum 45)
        $state = Test-GpuFree
        if ($state.Free) { break }
    }
    if ((Get-Date) -gt $deadline) { "GPU busy after $WaitMinutes minutes ($($state.Gpu) MiB, $($state.Renders) renders), not starting"; exit 2 }
    Start-Sleep -Seconds 20
}
"gpu used before: $($state.Gpu) MiB at $(Get-Date -Format T)"
Remove-Item $log -ErrorAction SilentlyContinue
Remove-Item "$out\airfield_candidate_C_*.png" -ErrorAction SilentlyContinue
$arguments = @('"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"',
    ('-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=' + $Map),
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldBuildSitePieces/airfield_candidates/C/capture_piece.py"',
    '-RTSWorldCampaignSaveSlot=AirfieldCandidateCCapture',
    '-unattended', '-nop4', '-nosound', '-nosplash', '-RenderOffscreen', '-NoLiveCoding',
    '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
$stop = (Get-Date).AddSeconds(660)
while ((Get-Date) -lt $stop -and -not $process.HasExited) { Start-Sleep -Seconds 4 }
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; "killed after timeout" }
Select-String -Path $log -Pattern "AIRFIELDCCAP|Fatal|Assertion|Traceback|AFC_AirfieldCamo|M_AirfieldC_Paint" | Select-Object -First 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
Get-ChildItem $out -Filter airfield_candidate_C_*.png | Select-Object Name, Length
