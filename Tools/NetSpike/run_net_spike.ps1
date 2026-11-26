# Runs the LAN multiplayer spike headless: a host and a client process on this PC (see Source/RTS_NetSpike).
# Needs a DebugGame (or Development) editor build that contains the RTS_NetSpike module.
#   .\run_net_spike.ps1
#   .\run_net_spike.ps1 -Configuration Development
# The host opens RTS_ToCopySmall as a listen server and plays the normal game; the client joins over loopback,
# orders one replicated tank to move and reports where it sees it. Both run without a renderer (-nullrhi), so the
# run uses no video memory. Exit code 0 only when the host printed RESULT PASS.
param(
	[string]$Configuration = 'DebugGame',
	[string]$OutputDirectory = '',
	# Seconds the host may take to load the map, start the match and settle the tank before the client starts.
	[int]$HostReadySeconds = 240,
	# Seconds both processes get after the client started; both also end themselves on their own limits.
	[int]$RunSeconds = 300
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
if ($OutputDirectory -eq '') {
	$OutputDirectory = 'D:\UE5Projects\RTS_Survival\Saved\NetSpike\' + (Get-Date -Format 'yyyyMMdd_HHmmss')
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$HostLog = Join-Path $OutputDirectory 'host.log'
$ClientLog = Join-Path $OutputDirectory 'client.log'

# Both start on the engine's empty map: no spike class may be needed before "Module Load" has run.
$SharedArguments = @($Project, '/Engine/Maps/Entry?game=/Script/Engine.GameModeBase', '-game', '-unattended', '-nop4',
	'-nosound', '-nullrhi', '-nosplash', '-NoTimeouts', '-multihome=127.0.0.1',
	'-DisablePlugins=Fab,BpGeneratorUltimate', '-ExecCmds="Module Load RTS_NetSpike"')

function Get-SpikeLines([string]$Log) {
	if (-not (Test-Path $Log)) { return @() }
	# Case sensitive: the command line and the log category spell the module name RTS_NetSpike.
	return @(Select-String -Path $Log -CaseSensitive -Pattern 'RTS_NETSPIKE' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('RTS_NETSPIKE')) })
}

Write-Output "== host -> $HostLog"
$HostProcess = Start-Process -FilePath $Exe -PassThru -WindowStyle Hidden `
	-ArgumentList ($SharedArguments + @('-NetSpikeRole=Host', "-abslog=$HostLog"))

# The host prints its listen port when the match runs and the tank stands still.
$Port = 0
$Deadline = (Get-Date).AddSeconds($HostReadySeconds)
while ((Get-Date) -lt $Deadline -and -not $HostProcess.HasExited -and $Port -eq 0) {
	Start-Sleep -Seconds 2
	$Listening = Get-SpikeLines $HostLog | Where-Object { $_ -match 'listening port=(\d+)' } | Select-Object -First 1
	if ($Listening -and $Listening -match 'listening port=(\d+)') { $Port = [int]$Matches[1] }
}

$ClientProcess = $null
if ($Port -ne 0) {
	Write-Output "== client -> $ClientLog (host port $Port)"
	$ClientProcess = Start-Process -FilePath $Exe -PassThru -WindowStyle Hidden `
		-ArgumentList ($SharedArguments + @('-NetSpikeRole=Client', "-NetSpikeAddress=127.0.0.1:$Port", "-abslog=$ClientLog"))
	$Deadline = (Get-Date).AddSeconds($RunSeconds)
	while ((Get-Date) -lt $Deadline -and -not ($HostProcess.HasExited -and $ClientProcess.HasExited)) {
		Start-Sleep -Seconds 2
	}
}

# A process still alive here hangs: it would keep the editor DLL locked for every other build.
foreach ($Process in @($HostProcess, $ClientProcess)) {
	if ($null -ne $Process -and -not $Process.HasExited) {
		Write-Output "Stopping hung process $($Process.Id)"
		Stop-Process -Id $Process.Id -Force
	}
}

Write-Output ''
Write-Output '-- host'
Get-SpikeLines $HostLog | Where-Object { $_ -notmatch ' sample ' } | ForEach-Object { Write-Output $_ }
Write-Output '-- client'
Get-SpikeLines $ClientLog | Where-Object { $_ -notmatch ' sample ' } | ForEach-Object { Write-Output $_ }

foreach ($Log in @($HostLog, $ClientLog)) {
	if (-not (Test-Path $Log)) { continue }
	$Errors = @(Select-String -Path $Log -Pattern '(Error:|Fatal error|Unhandled Exception|Assertion failed|Ensure condition failed)')
	Write-Output ''
	Write-Output "-- $(Split-Path $Log -Leaf): $($Errors.Count) error lines, first 12:"
	$Errors | Select-Object -First 12 | ForEach-Object { Write-Output $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
}

$Result = Get-SpikeLines $HostLog | Where-Object { $_ -match 'RTS_NETSPIKE host RESULT' } | Select-Object -Last 1
Write-Output ''
if ($null -eq $Result) {
	Write-Output "RTS_NETSPIKE host RESULT FAIL reasons=no_result_line host_exit=$($HostProcess.ExitCode) port=$Port"
	exit 1
}
Write-Output $Result
if ($Result -match 'RESULT PASS') { exit 0 } else { exit 1 }
