# Runs the Soviet refinery map test headless (see Source/Docs/EnemyAI/SovietRefinery.md).
# Needs a DebugGame (or Development) editor build with USovietRefineryTestSubsystem.
#   .\run_soviet_refinery_test.ps1                       # both designs, pictures, RTS_ToCopySmall
#   .\run_soviet_refinery_test.ps1 -Design A1 -NoCapture # one design, -nullrhi
# Exit code 0 only when the log ends with RTS_SOVREF_TEST RESULT PASS.
param(
	[string]$Design = 'Both',
	[string]$At = '',
	[string]$Map = '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall',
	[string]$Configuration = 'DebugGame',
	[switch]$NoCapture,
	[string]$OutputDirectory = ''
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
if ($OutputDirectory -eq '') {
	$OutputDirectory = 'D:\UE5Projects\RTS_Survival\Saved\SovietRefineryTest\' + (Get-Date -Format 'yyyyMMdd_HHmmss')
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$Log = Join-Path $OutputDirectory 'soviet_refinery.log'

$Arguments = @($Project, $Map, '-game', '-unattended', '-nop4', '-nosound',
	'-DisablePlugins=Fab,BpGeneratorUltimate', '-SovietRefineryValidate', "-SovietRefineryDesign=$Design", "-abslog=$Log")
if ($NoCapture) {
	$Arguments += '-nullrhi'
} else {
	$Arguments += @('-RenderOffscreen', "-SovietRefineryCapture=$OutputDirectory")
}
if ($At -ne '') { $Arguments += "-SovietRefineryAt=$At" }

Write-Output "== soviet refinery test -> $Log"
$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
$Process.WaitForExit()

$Result = $null
if (Test-Path $Log) {
	Select-String -Path $Log -Pattern 'RTS_SOVREF_TEST (check|spawned|natural)|SOVREF_(ACTIVE|HARVESTER|CONVERT)|RTS_SOVCRANE (ready|.*outside)' |
		ForEach-Object { $_.Line.Substring($_.Line.IndexOf('] ') + 2) } | Select-Object -First 120
	$Result = Select-String -Path $Log -Pattern 'RTS_SOVREF_TEST RESULT (PASS|FAIL)' | Select-Object -Last 1
}
$Line = if ($null -eq $Result) { "RTS_SOVREF_TEST RESULT FAIL reasons=no_result_line exit_code=$($Process.ExitCode) log=$Log" }
        else { $Result.Line.Substring($Result.Line.IndexOf('RTS_SOVREF_TEST')) }
Write-Output ''
Write-Output $Line
if ($Line -match '^RTS_SOVREF_TEST RESULT PASS') { exit 0 } else { exit 1 }
