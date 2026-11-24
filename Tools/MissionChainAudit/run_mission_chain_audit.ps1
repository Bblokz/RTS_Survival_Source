# Walks the Blueprint mission chains of the two-player mission maps headless and audits the enemy AI they drive
# (see Source/RTS_Survival/Utils/RTSMapTests/MissionChainAudit/MissionChainAuditSubsystem.h).
# Needs a DebugGame (or Development) editor build with map tests. One map runs at a time.
#   .\run_mission_chain_audit.ps1                         # all mission maps
#   .\run_mission_chain_audit.ps1 -Maps Ger_FirstMission -Seconds 600
param(
	[string[]]$Maps = @(),
	[int]$Seconds = 420,
	[int]$StepSeconds = 25,
	[string]$Configuration = "DebugGame",
	[string]$OutputDirectory = ""
)

$AllMaps = [ordered]@{
	'Ger_FirstMission'       = '/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission'
	'Ger_MarderStugMission'  = '/Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission'
	'Ger_Trains'             = '/Game/RTS_Survival/Maps/GerMissions/Ger_Trains'
	'RTS_GameStartTutorial'  = '/Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_GameStartTutorial'
	'UT_EnemyAI'             = '/Game/RTS_Survival/Maps/UnitTests/UT_EnemyAI'
}
if ($Maps.Count -eq 0) { $Maps = @($AllMaps.Keys) }

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
if ($OutputDirectory -eq "") {
	$OutputDirectory = "D:\UE5Projects\RTS_Survival\Saved\MissionChainAudit\" + (Get-Date -Format 'yyyyMMdd_HHmmss')
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null

$AllPassed = $true
foreach ($MapName in $Maps) {
	$MapDir = Join-Path $OutputDirectory $MapName
	New-Item -ItemType Directory -Force $MapDir | Out-Null
	$Log = Join-Path $MapDir 'audit.log'
	# Not -nullrhi: the fog of war (enemy vision) needs a renderer.
	$Arguments = @($Project, $AllMaps[$MapName], '-game', '-unattended', '-nop4', '-nosound', '-RenderOffscreen',
		'-windowed', '-ResX=960', '-ResY=540', '-DisablePlugins=Fab,BpGeneratorUltimate', '-NoLiveCoding',
		'-RTSMapSmokeTest', "-RTSMapSmokeTestSeconds=$Seconds", '-RTSMapSmokeTestPIESettings',
		'-RTSMissionChainAudit', "-RTSMissionChainAuditDir=$MapDir", "-RTSMissionChainAuditStep=$StepSeconds",
		"-abslog=$Log")
	Write-Output "== $MapName -> $MapDir"
	$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
	$Process.WaitForExit()
	$Results = Select-String -Path $Log -Pattern 'RTS_MCA RESULT|RTS_MAP_SMOKE_TEST RESULT' | ForEach-Object {
		$_.Line.Substring([Math]::Max(0, $_.Line.IndexOf('RTS_'))) }
	if ($Results.Count -eq 0) { $Results = @("RTS_MCA RESULT FAIL reasons=no_result_line map=$MapName") }
	$Results | ForEach-Object { Write-Output $_ }
	if (($Results | Where-Object { $_ -match 'RTS_MCA RESULT FAIL' }).Count -gt 0) { $AllPassed = $false }
}
Write-Output "Summaries under $OutputDirectory\<map>\summary.md"
if (-not $AllPassed) { exit 1 }
