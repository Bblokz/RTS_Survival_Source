# Builds the DebugGame editor module (the one the headless commandlets use) behind the shared flora lock.
# Refuses when any running process has a project DebugGame DLL loaded. Never touches other processes.
$lock = 'D:\UE5Projects\RTS_Survival\Saved\OreMountains\flora\ue.lock'
$log = 'D:\UE5Projects\RTS_Survival\Saved\OreMountains\metal\build_debuggame.log'
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
$deadline = (Get-Date).AddMinutes(30)
while ($true) {
    try { New-Item -ItemType Directory $lock -ErrorAction Stop | Out-Null; break }
    catch { if ((Get-Date) -gt $deadline) { 'LOCK: still held after 30 min'; exit 4 }; Start-Sleep -Seconds 20 }
}
Set-Content -Path "$lock\owner.txt" -Value "ore metal resources: DebugGame build $(Get-Date -Format s)"
try {
    $holders = @()
    foreach ($p in Get-Process) {
        if ($p.ProcessName -notlike 'Unreal*' -and $p.ProcessName -notlike 'RTS_*') { continue }
        try { foreach ($m in $p.Modules) { if ($m.ModuleName -like '*RTS_*Win64-DebugGame*') { $holders += "$($p.Id) $($p.ProcessName) $($m.ModuleName)" } } } catch { }
        if ($p.ProcessName -like '*DebugGame*') { $holders += "$($p.Id) $($p.ProcessName)" }
    }
    if ($holders.Count -gt 0) { 'DLL HELD, not building:'; $holders; exit 5 }
    & 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Build\BatchFiles\Build.bat' RTS_SurvivalEditor Win64 DebugGame '-Project=D:\UE5Projects\RTS_Survival\RTS_Survival.uproject' -WaitMutex -NoHotReload *> $log
    "build exit=$LASTEXITCODE"
}
finally { Remove-Item -Recurse -Force $lock }
Select-String -Path $log -Pattern 'error|Result:|Total execution|warning C' | Select-Object -Last 25 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(400, $_.Line.Length)) }
