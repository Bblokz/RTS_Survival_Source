# Runs the enemy (player 2) harvesting map test headless (see Source/Docs/Resources/EnemyHarvesting.md).
# Needs a DebugGame (or Development) editor build that contains UEnemyHarvestTestSubsystem.
#   .\run_enemy_harvest_test.ps1                          # RTS_ToCopySmall, the radixite field nearest the map origin
#   .\run_enemy_harvest_test.ps1 -At 16796,-15808         # the radixite field nearest this X,Y
# Exit code 0 only when the log ends with RTS_ENEMY_HARVEST_TEST RESULT PASS and none of the old player-1-only
# drop-off errors appear.
param(
	[string]$At = '',
	[string]$Map = '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall',
	[string]$Configuration = 'DebugGame',
	[string]$OutputDirectory = ''
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
if ($OutputDirectory -eq '') {
	$OutputDirectory = 'D:\UE5Projects\RTS_Survival\Saved\EnemyHarvestTest\' + (Get-Date -Format 'yyyyMMdd_HHmmss')
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$Log = Join-Path $OutputDirectory 'enemy_harvest.log'

$Arguments = @($Project, $Map, '-game', '-unattended', '-nop4', '-nosound', '-nullrhi',
	'-DisablePlugins=Fab,BpGeneratorUltimate', '-EnemyHarvestValidate', "-abslog=$Log")
if ($At -ne '') { $Arguments += "-EnemyHarvestAt=$At" }

Write-Output "== enemy harvest test -> $Log"
$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
$Process.WaitForExit()

$Result = $null
$QuietViolations = @()
if (Test-Path $Log) {
	Select-String -Path $Log -Pattern 'RTS_ENEMY_HARVEST_TEST (check|layout|baseline|both_delivered|drained|retarget|forced)' |
		ForEach-Object { $_.Line.Substring($_.Line.IndexOf('RTS_ENEMY_HARVEST_TEST')) }
	$Result = Select-String -Path $Log -Pattern 'RTS_ENEMY_HARVEST_TEST RESULT (PASS|FAIL)' | Select-Object -Last 1
	# The errors an AI drop-off used to raise (it must now be quiet).
	$QuietViolations = @(Select-String -Path $Log -Pattern @(
		'Enemy ResourceDropOffs not supported',
		'DropOff is not able to update visuals',
		'UResourceDropOff::GetIsValidPlayerResourceManager',
		'Harvester owner has no RTS component',
		'to type: IResourceStorageOwner'))
}
if ($null -eq $Result) {
	$Line = "RTS_ENEMY_HARVEST_TEST RESULT FAIL reasons=no_result_line exit_code=$($Process.ExitCode) log=$Log"
} else {
	$Line = $Result.Line.Substring($Result.Line.IndexOf('RTS_ENEMY_HARVEST_TEST'))
}
if ($QuietViolations.Count -gt 0) {
	Write-Output "Drop-off errors in the log ($($QuietViolations.Count)):"
	$QuietViolations | Select-Object -First 5 | ForEach-Object { $_.Line }
	$Line = "RTS_ENEMY_HARVEST_TEST RESULT FAIL reasons=dropoff_errors_logged ($Line)"
}
Write-Output ''
Write-Output $Line
if ($Line -match '^RTS_ENEMY_HARVEST_TEST RESULT PASS') { exit 0 } else { exit 1 }
