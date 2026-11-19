# Runs the enemy building trainer test headless (see Source/Docs/EnemyAI/EnemyBuildingTraining.md).
# Needs a DebugGame (or Development) editor build that contains UEnemyBuildingTrainerTestSubsystem.
#   .\run_enemy_building_trainer_test.ps1                                   # all seven Soviet trainer buildings
#   .\run_enemy_building_trainer_test.ps1 -Buildings Factory -Capture       # one building + PNG frames
#   .\run_enemy_building_trainer_test.ps1 -AddMissing                       # core check before the BPs have a trainer
# Labels: Barracks, BarracksConcrete, LargeTent, LargeTentSoviet, Factory, AssemblyYard, PlatformFactory.
# Exit code 0 only when the log ends with RTS_EBT RESULT PASS.
param(
	[string]$Buildings = '',
	[switch]$Capture,
	[switch]$AddMissing,
	# Spawns every unit the AI can train and checks each comes out as itself (no buildings, no training).
	[switch]$SpawnerAudit,
	# Two units per building, exit-view frames around each unit's appearance and spawnqa measurements.
	[switch]$SpawnQA,
	[string]$Exposure = '',
	[string]$At = '',
	# ESquadSubtype / ETankSubtype names the buildings train instead of Squad_Rus_Mosin / Tank_BT7.
	[string]$Squad = '',
	[string]$Tank = '',
	[string]$Configuration = 'DebugGame',
	[string]$OutputDirectory = ''
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
$Map = '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall'
if ($OutputDirectory -eq '') {
	$OutputDirectory = 'D:\UE5Projects\RTS_Survival\Saved\EnemyBuildingTrainer\' + (Get-Date -Format 'yyyyMMdd_HHmmss')
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$Log = Join-Path $OutputDirectory 'trainer.log'

$Arguments = @($Project, $Map, '-game', '-unattended', '-nop4', '-nosound',
	# Not -nullrhi: the captures need a renderer.
	'-RenderOffscreen', '-windowed', '-ResX=640', '-ResY=360', '-DisablePlugins=Fab,BpGeneratorUltimate',
	'-EnemyBuildingTrainerTest', "-EnemyBuildingTrainerTestDir=$OutputDirectory", "-abslog=$Log")
if ($Buildings -ne '') { $Arguments += "-EnemyBuildingTrainerTestBuildings=$Buildings" }
if ($Capture) { $Arguments += '-EnemyBuildingTrainerTestCapture' }
if ($AddMissing) { $Arguments += '-EnemyBuildingTrainerTestAddMissing' }
if ($SpawnerAudit) { $Arguments += '-EnemyBuildingTrainerTestSpawnerAudit' }
if ($SpawnQA) { $Arguments += '-EnemyBuildingTrainerTestSpawnQA' }
if ($Exposure -ne '') { $Arguments += "-EnemyBuildingTrainerTestExposure=$Exposure" }
if ($At -ne '') { $Arguments += "-EnemyBuildingTrainerTestAt=$At" }
if ($Squad -ne '') { $Arguments += "-EnemyBuildingTrainerTestSquad=$Squad" }
if ($Tank -ne '') { $Arguments += "-EnemyBuildingTrainerTestTank=$Tank" }

Write-Output "== enemy building trainer test -> $OutputDirectory"
$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
$Process.WaitForExit()

$Result = $null
if (Test-Path $Log) {
	Select-String -Path $Log -Pattern 'RTS_EBT (job|case|routed|real_think|cancel|audit|spawnqa|.*WARNING)' |
		ForEach-Object { $_.Line.Substring($_.Line.IndexOf('RTS_EBT')) }
	$Result = Select-String -Path $Log -Pattern 'RTS_EBT RESULT (PASS|FAIL)' | Select-Object -Last 1
}
if ($null -eq $Result) {
	$Line = "RTS_EBT RESULT FAIL reasons=no_result_line exit_code=$($Process.ExitCode) log=$Log"
} else {
	$Line = $Result.Line.Substring($Result.Line.IndexOf('RTS_EBT'))
}
Write-Output ''
Write-Output $Line
Write-Output "Summary: $OutputDirectory\summary.md"
if ($Line -match 'RTS_EBT RESULT PASS') { exit 0 } else { exit 1 }
