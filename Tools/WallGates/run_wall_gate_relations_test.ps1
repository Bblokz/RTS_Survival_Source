# Runs the four-player wall gate test headless (UWallGateRelationsTestSubsystem, map tests only).
# Needs a DebugGame (or Development) editor build that contains the subsystem.
#   .\run_wall_gate_relations_test.ps1
#   .\run_wall_gate_relations_test.ps1 -At "16796,-15808" -Configuration Development
# Spawns a Soviet wall gate of player 2 and a Pz IV H of players 1, 2 and 3 on RTS_ToCopySmall and checks: the gate
# opens for player 2, stays shut for hostile players 3 and 1, opens for player 3 once players 2 and 3 share a team,
# per-player path planning through the closed doorway, weapon traces at the closed leaf, and a capture by player 4.
# Exit code 0 only when the log has RTS_WALLGATE RESULT PASS.
param(
	[string]$Configuration = 'DebugGame',
	[string]$OutputDirectory = '',
	# Doorway position "X,Y" in cm (default in C++: the vehicle duel bench's flat field on RTS_ToCopySmall).
	[string]$At = '',
	[string]$Map = '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall'
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
if ($OutputDirectory -eq '') {
	$OutputDirectory = 'D:\UE5Projects\RTS_Survival\Saved\WallGateRelations\' + (Get-Date -Format 'yyyyMMdd_HHmmss')
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$Log = Join-Path $OutputDirectory 'wall_gate_relations.log'

$Arguments = @($Project, $Map, '-game', '-unattended', '-nop4', '-nosound',
	'-RenderOffscreen', '-windowed', '-ResX=640', '-ResY=360', '-DisablePlugins=Fab,BpGeneratorUltimate',
	'-WallGateRelationsTest', "-WallGateTestDir=$OutputDirectory", "-abslog=$Log")
if ($At -ne '') { $Arguments += "-WallGateTestAt=$At" }

Write-Output "== Wall gate relations -> $OutputDirectory"
$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
$Process.WaitForExit()

$Result = $null
if (Test-Path $Log) {
	Select-String -Path $Log -Pattern 'RTS_WALLGATE (CHECK|step|spawned)|RTS_WALL_GATE' |
		ForEach-Object { $_.Line.Substring($_.Line.IndexOf('RTS_WALL')) }
	$Result = Select-String -Path $Log -Pattern 'RTS_WALLGATE RESULT (PASS|FAIL)' | Select-Object -Last 1
}
if ($null -eq $Result) {
	# A crash or a hang killed from outside leaves no RESULT line: that is a failure too.
	$Line = "RTS_WALLGATE RESULT FAIL reasons=no_result_line exit_code=$($Process.ExitCode) log=$Log"
} else {
	$Line = $Result.Line.Substring($Result.Line.IndexOf('RTS_WALLGATE'))
}
Write-Output ''
Write-Output "Summary: $OutputDirectory\summary.md"
Write-Output $Line
if ($Line -match 'RTS_WALLGATE RESULT PASS') { exit 0 } else { exit 1 }
