# Headless Unreal python commandlet for the mineral-flora trees, behind the shared flora lock.
# usage: powershell -File run_ue_cmd.ps1 -Script <path.py> [-ScriptArgs "a b"] [-Tag name] [-NullRhi]
# Takes D:\UE5Projects\RTS_Survival\Saved\OreMountains\flora\ue.lock (mkdir is atomic; waits up to 30 min), removes it after.
# Never touches other Unreal processes. Kills only its own commandlet on timeout.
param([Parameter(Mandatory = $true)][string]$Script, [string]$ScriptArgs = '', [string]$Tag = '', [switch]$NullRhi, [int]$Limit = 1500)
$lock = 'D:\UE5Projects\RTS_Survival\Saved\OreMountains\flora\ue.lock'
$logs = 'D:\UE5Projects\RTS_Survival\Saved\OreMountains\flora\trees\logs'
New-Item -ItemType Directory -Force $logs | Out-Null
if (-not $Tag) { $Tag = [IO.Path]::GetFileNameWithoutExtension($Script) }
$log = "$logs\$Tag.log"
$deadline = (Get-Date).AddMinutes(30)
while ($true) {
    try { New-Item -ItemType Directory $lock -ErrorAction Stop | Out-Null; break }
    catch {
        if ((Get-Date) -gt $deadline) { 'LOCK: still held after 30 min, giving up'; exit 4 }
        Start-Sleep -Seconds 20
    }
}
Set-Content -Path "$lock\owner.txt" -Value "trees builder A: $Tag $(Get-Date -Format s)"
try {
    $exe = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe'
    $sa = ($Script.Replace('\', '/') + ' ' + $ScriptArgs).Trim()
    $argList = '"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject" -run=pythonscript -script="' + $sa + '" -unattended -nosplash -NoSound -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="' + $log + '"'
    if ($NullRhi) { $argList += ' -nullrhi' }
    $p = Start-Process $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
    if (-not $p.WaitForExit($Limit * 1000)) { 'timeout, killing own commandlet'; Stop-Process -Id $p.Id -Force }
    "exit=$($p.ExitCode) log=$log"
}
finally {
    Remove-Item -Recurse -Force $lock
}
Select-String -Path $log -Pattern 'FLORA_|Traceback|LogPython: Error|Error: .*\.py' | Select-Object -Last 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(500, $_.Line.Length)) }
