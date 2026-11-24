# Runs flora_tree_test.py in ONE throwaway DebugGame editor on Maps/UnitTests/RTS_ToCopySmall (never saved, never the
# shared editor), behind the shared flora lock. Refuses to start below MinFreeMiB of free video memory (waits up to
# WaitMin minutes for it). Kills only its own editor on timeout.
param([string]$Name = 'trees1', [int]$Limit = 760, [int]$MinFreeMiB = 4000, [int]$WaitMin = 30)
$lock = 'D:\UE5Projects\RTS_Survival\Saved\OreMountains\flora\ue.lock'
$root = 'D:\UE5Projects\RTS_Survival\Saved\OreMountains\flora\trees\test'
New-Item -ItemType Directory -Force $root | Out-Null
$log = "$root\$Name.log"
$deadline = (Get-Date).AddMinutes($WaitMin)
while ($true) {
    $free = [int]((nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits) | Select-Object -First 1)
    if ($free -ge $MinFreeMiB) {
        try { New-Item -ItemType Directory $lock -ErrorAction Stop | Out-Null; break } catch { }
    }
    if ((Get-Date) -gt $deadline) { "GAVE UP: free video memory $free MiB (need $MinFreeMiB) or lock held after $WaitMin min"; exit 4 }
    Start-Sleep -Seconds 30
}
Set-Content -Path "$lock\owner.txt" -Value "trees builder A: editor test $Name $(Get-Date -Format s)"
"lock taken, free video memory $free MiB"
try {
    $exe = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe'
    $argList = '"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject" "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/OreMineralFlora/trees/flora_tree_test.py" -ResX=1280 -ResY=720 -WINDOWED -unattended -nosplash -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="' + $log + '"'
    $p = Start-Process $exe -ArgumentList $argList -PassThru
    if (-not $p.WaitForExit($Limit * 1000)) { 'timeout, killing own editor'; Stop-Process -Id $p.Id -Force }
    "exit code $($p.ExitCode)"
}
finally {
    Remove-Item -Recurse -Force $lock
}
Select-String -Path $log -Pattern 'FLORATEST|Script Stack|Fatal|Ran out of|OutOfMemory' | Select-Object -Last 120 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(700, $_.Line.Length)) }
