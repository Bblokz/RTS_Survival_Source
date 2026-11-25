# Runs the vehicle run-over test headless (UVehicleCrushTestSubsystem, map tests only).
# Needs a DebugGame (or Development) editor build that contains the subsystem.
#   .\run_vehicle_crush_test.ps1
#   .\run_vehicle_crush_test.ps1 -At "16796,-15808" -Configuration Development
# A held-fire Pz IV H of player 1 drives through Mosin squads of player 2 (hostile), 1 (own) and 3 (allied) on
# RTS_ToCopySmall: only hostile soldiers may be run over, some must be, and reacting soldiers must dodge.
# Exit code 0 only when the log has RTS_VEHICLE_CRUSH RESULT PASS.
param(
	[string]$Configuration = 'DebugGame',
	[string]$At = '',
	[string]$Map = '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall'
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
$OutputDirectory = 'D:\UE5Projects\RTS_Survival\Saved\VehicleCrushTest\' + (Get-Date -Format 'yyyyMMdd_HHmmss')
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$Log = Join-Path $OutputDirectory 'vehicle_crush.log'

$Arguments = @($Project, $Map, '-game', '-unattended', '-nop4', '-nosound',
	'-RenderOffscreen', '-windowed', '-ResX=640', '-ResY=360',
	'-DisablePlugins=Fab,BpGeneratorUltimate', '-VehicleCrushTest', "-abslog=$Log")
if ($At -ne '') { $Arguments += "-VehicleCrushTestAt=$At" }

Write-Output "== Vehicle crush -> $Log"
$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
$Process.WaitForExit()

$Result = $null
if (Test-Path $Log) {
	Select-String -Path $Log -Pattern 'RTS_VEHICLE_CRUSH' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('RTS_VEHICLE_CRUSH')) }
	$Result = Select-String -Path $Log -Pattern 'RTS_VEHICLE_CRUSH RESULT (PASS|FAIL)' | Select-Object -Last 1
}
if ($null -eq $Result) {
	Write-Output "RTS_VEHICLE_CRUSH RESULT FAIL reasons=no_result_line exit_code=$($Process.ExitCode) log=$Log"
	exit 1
}
if ($Result.Line -match 'RESULT PASS') { exit 0 } else { exit 1 }
