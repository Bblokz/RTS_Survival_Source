# Runs the Realistic vehicle duel bench headless (see Source/Docs/Balance/VehicleDuelBench.md).
# Needs a DebugGame (or Development) editor build that contains UVehicleDuelBenchSubsystem.
#   .\run_vehicle_duel_bench.ps1                              # all scenarios, 3 repeats each
#   .\run_vehicle_duel_bench.ps1 -Scenario Tiger_vs_KV1 -Repeats 5
param(
	[string]$Scenario = "",
	[int]$Repeats = 3,
	[string]$Configuration = "DebugGame",
	[string]$OutputDirectory = ""
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
if ($OutputDirectory -eq "") {
	$OutputDirectory = "D:\UE5Projects\RTS_Survival\Saved\VehicleDuelBench\" + (Get-Date -Format 'yyyyMMdd_HHmmss')
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$Log = Join-Path $OutputDirectory 'bench.log'

$Arguments = @($Project, '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall', '-game', '-unattended', '-nop4', '-nosound',
	# Not -nullrhi: the enemy's fog of war vision needs a renderer, without it Soviet turrets never see a target.
	'-RenderOffscreen', '-windowed', '-ResX=640', '-ResY=360', '-DisablePlugins=Fab,BpGeneratorUltimate', '-VehicleDuelBench', "-VehicleDuelBenchRepeats=$Repeats",
	"-VehicleDuelBenchDir=$OutputDirectory", "-abslog=$Log")
if ($Scenario -ne "") { $Arguments += "-VehicleDuelBenchScenario=$Scenario" }

$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
$Process.WaitForExit()
Select-String -Path $Log -Pattern 'RTS_VDB (run |run_done|RESULT)' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('RTS_VDB')) }
Write-Output "Summary: $OutputDirectory\summary.md"
Write-Output "Hits:    $OutputDirectory\hits.csv"
