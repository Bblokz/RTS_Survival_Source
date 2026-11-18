# Runs the four-player skirmish playtest headless (see Source/Docs/Multiplayer/FourPlayerSkirmish.md).
# Needs a DebugGame (or Development) editor build that contains UFourPlayerSkirmishSubsystem.
#   .\run_four_player_skirmish.ps1                       # free for all, then teams
#   .\run_four_player_skirmish.ps1 -Phase Teams
#   .\run_four_player_skirmish.ps1 -Phase FreeForAll -Seconds 90 -Side 3000
# One editor process per phase, one after another. Exit code 0 only when every phase printed RESULT PASS.
param(
	[ValidateSet('FreeForAll', 'Teams', 'Both')]
	[string]$Phase = 'Both',
	[string]$Configuration = 'DebugGame',
	[string]$OutputDirectory = '',
	# Game seconds before the fight is stopped and judged (default in C++: 120).
	[int]$Seconds = 0,
	# Side of the square the four groups stand on, in cm (default in C++: 2600).
	[int]$Side = 0,
	# Arena centre "X,Y" in cm (default in C++: the vehicle duel bench's flat field on RTS_ToCopySmall).
	[string]$At = ''
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
$Map = '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall'
if ($OutputDirectory -eq '') {
	$OutputDirectory = 'D:\UE5Projects\RTS_Survival\Saved\FourPlayerSkirmish\' + (Get-Date -Format 'yyyyMMdd_HHmmss')
}
$Phases = if ($Phase -eq 'Both') { @('FreeForAll', 'Teams') } else { @($Phase) }

$AllPassed = $true
$ResultLines = @()
foreach ($RunPhase in $Phases) {
	$PhaseDirectory = Join-Path $OutputDirectory $RunPhase
	New-Item -ItemType Directory -Force $PhaseDirectory | Out-Null
	$Log = Join-Path $PhaseDirectory 'skirmish.log'
	$Arguments = @($Project, $Map, '-game', '-unattended', '-nop4', '-nosound',
		# Not -nullrhi: the fog of war needs a renderer, without it the human player's units never see a target.
		'-RenderOffscreen', '-windowed', '-ResX=640', '-ResY=360', '-DisablePlugins=Fab,BpGeneratorUltimate',
		'-FourPlayerSkirmish', "-FourPlayerSkirmishPhase=$RunPhase", "-FourPlayerSkirmishDir=$PhaseDirectory",
		"-abslog=$Log")
	if ($Seconds -gt 0) { $Arguments += "-FourPlayerSkirmishSeconds=$Seconds" }
	if ($Side -gt 0) { $Arguments += "-FourPlayerSkirmishSide=$Side" }
	if ($At -ne '') { $Arguments += "-FourPlayerSkirmishAt=$At" }

	Write-Output "== $RunPhase -> $PhaseDirectory"
	$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
	$Process.WaitForExit()

	$Result = $null
	if (Test-Path $Log) {
		Select-String -Path $Log -Pattern 'RTS_FPS (spawned units|setup_gate|reach|FRIENDLY_DAMAGE|NON_HOSTILE_TARGET|OWNER_DRIFT|matrix)' |
			ForEach-Object { $_.Line.Substring($_.Line.IndexOf('RTS_FPS')) }
		$Result = Select-String -Path $Log -Pattern 'RTS_FPS RESULT (PASS|FAIL)' | Select-Object -Last 1
	}
	if ($null -eq $Result) {
		# A crash or a hang killed from outside leaves no RESULT line: that is a failure too.
		$Line = "RTS_FPS RESULT FAIL reasons=no_result_line phase=$RunPhase exit_code=$($Process.ExitCode) log=$Log"
	} else {
		$Line = $Result.Line.Substring($Result.Line.IndexOf('RTS_FPS'))
	}
	if ($Line -notmatch 'RTS_FPS RESULT PASS') { $AllPassed = $false }
	$ResultLines += $Line
	Write-Output "Summary: $PhaseDirectory\summary.md"
	Write-Output "Hits:    $PhaseDirectory\hits.csv"
}

Write-Output ''
$ResultLines | ForEach-Object { Write-Output $_ }
if ($AllPassed) { exit 0 } else { exit 1 }
