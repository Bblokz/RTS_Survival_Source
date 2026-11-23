# Runs metal_harvest_test.py in ONE throwaway DebugGame editor on Maps/UnitTests/RTS_ToCopySmall (never saved, never
# the shared editor), behind the shared flora lock. Kills only its own editor on timeout.
#   -NullRhi   : no renderer, no pictures, needs no video memory (gameplay test only)
#   otherwise  : rendered, takes the pictures too; refuses to start below MinFreeMiB of free video memory
param([string]$Name = 'metal1', [string]$Resources = 'Blutbarren', [switch]$NullRhi, [switch]$LookOnly, [int]$Limit = 780, [int]$MinFreeMiB = 4000, [int]$WaitMin = 30)
$lock = 'D:\UE5Projects\RTS_Survival\Saved\OreMountains\flora\ue.lock'
$root = 'D:\UE5Projects\RTS_Survival\Saved\OreMountains\metal\test'
New-Item -ItemType Directory -Force $root | Out-Null
$log = "$root\$Name.log"
$deadline = (Get-Date).AddMinutes($WaitMin)
while ($true) {
    $free = [int]((nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits) | Select-Object -First 1)
    if ($NullRhi -or $free -ge $MinFreeMiB) {
        try { New-Item -ItemType Directory $lock -ErrorAction Stop | Out-Null; break } catch { }
    }
    if ((Get-Date) -gt $deadline) { "GAVE UP: free video memory $free MiB (need $MinFreeMiB) or lock held after $WaitMin min"; exit 4 }
    Start-Sleep -Seconds 30
}
Set-Content -Path "$lock\owner.txt" -Value "ore metal resources: editor test $Name $(Get-Date -Format s)"
"lock taken, free video memory $free MiB, nullrhi=$NullRhi"
try {
    $env:ORE_METAL_RESOURCES = $Resources
    $env:ORE_METAL_LOOK_ONLY = $(if ($LookOnly) { '1' } else { '0' })
    $env:ORE_METAL_RUN = $Name
    $env:ORE_METAL_RENDER = $(if ($NullRhi) { '0' } else { '1' })
    $exe = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe'
    $argList = '"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject" "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/OreMetalResources/metal_harvest_test.py" -ResX=1280 -ResY=720 -WINDOWED -unattended -nosplash -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="' + $log + '"'
    if ($NullRhi) { $argList += ' -nullrhi' }
    $p = Start-Process $exe -ArgumentList $argList -PassThru
    if (-not $p.WaitForExit($Limit * 1000)) { 'timeout, killing own editor'; Stop-Process -Id $p.Id -Force }
    "exit code $($p.ExitCode)"
}
finally {
    Remove-Item -Recurse -Force $lock
}
Select-String -Path $log -Pattern 'METALTEST|Script Stack|Fatal error|Ran out of|OutOfMemory' | Select-Object -Last 90 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(900, $_.Line.Length)) }
