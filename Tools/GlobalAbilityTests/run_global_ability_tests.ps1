# Runs the global ability test (-RTSGlobalAbilityTest) of a staged RTS_SurvivalShippingGlobalAbilityTests package on
# one or more maps. Each map gets its own progress file (Saved/GlobalAbilityTests/<RunId>.txt); when the game crashes
# or hangs the script keeps the log and crash folder and relaunches, and the runner continues after the wave that was
# running (blaming that wave). Usage:
#   powershell -File run_global_ability_tests.ps1 -Maps Ger_FirstMission,Ger_Trains [-Mode Both] [-Fresh]
#     [-StressSeconds 600] [-Extra '-RTSGATestOnly=GA_GerE50Drop'] [-RunTag v1]
param(
	[string[]]$Maps = @('RTS_ToCopySmall'),
	[string]$Mode = 'Both',
	[int]$StressSeconds = 600,
	[int]$Wave = 3,
	[string]$Extra = '',
	[string]$RunTag = 'r1',
	[switch]$Fresh,
	[int]$MaxLaunches = 12,
	[int]$HangMinutes = 25,
	[string]$StageDir = 'E:\RR_test\FInalWindows',
	[string]$LogDir = 'E:\RR_test\GA_logs'
)

$MapPaths = @{
	'Ger_FirstMission'      = '/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission'
	'Ger_MarderStugMission' = '/Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission'
	'Ger_DestroyT35'        = '/Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35'
	'Ger_Trains'            = '/Game/RTS_Survival/Maps/GerMissions/Ger_Trains'
	'RedLine'               = '/Game/RTS_Survival/Maps/GerMissions/RedLine'
	'RTS_GameStartTutorial' = '/Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_GameStartTutorial'
	'RTS_MechanicsTutorial' = '/Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_MechanicsTutorial'
	'SM_Desert_01'          = '/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01'
	'SM_Kessel_01'          = '/Game/RTS_Survival/SurvivalMode/Maps/SM_Kessel_01'
	'RTS_ToCopySmall'       = '/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall'
	'UT_Nomadics'           = '/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics'
}

# powershell -File passes 'A,B' as one string: split it.
$Maps = @($Maps | ForEach-Object { $_ -split ',' } | Where-Object { $_ })

$Exe = Join-Path $StageDir 'Windows\RTS_Survival\Binaries\Win64\RTS_SurvivalShippingGlobalAbilityTests-Win64-Shipping.exe'
if (-not (Test-Path $Exe)) { throw "No test exe at $Exe" }
$Saved = Join-Path $env:LOCALAPPDATA 'RTS_Survival\Saved'
$ProgressDir = Join-Path $Saved 'GlobalAbilityTests'
New-Item -ItemType Directory -Force $LogDir, $ProgressDir | Out-Null

foreach ($Map in $Maps) {
	$MapPath = $MapPaths[$Map]
	if (-not $MapPath) { $MapPath = $Map; $Map = Split-Path $Map -Leaf }
	$RunId = "${Map}_$RunTag"
	$Progress = Join-Path $ProgressDir "$RunId.txt"
	if ($Fresh -and (Test-Path $Progress)) { Remove-Item $Progress }
	for ($Launch = 1; $Launch -le $MaxLaunches; $Launch++) {
		if ((Test-Path $Progress) -and (Select-String -Path $Progress -Pattern '^RUN_DONE' -Quiet)) { break }
		$Log = Join-Path $LogDir "${RunId}_launch$Launch.log"
		$ArgList = @($MapPath, '-RTSGlobalAbilityTest', "-RTSGATestMode=$Mode", "-RTSGATestRunId=$RunId",
			"-RTSGATestStressSeconds=$StressSeconds", "-RTSGATestWave=$Wave", '-RTSTestExitOnComplete',
			'-windowed', '-ResX=1280', '-ResY=720', '-unattended', '-nosplash', "-abslog=$Log") + ($Extra -split ' ' | Where-Object { $_ })
		Write-Host "[$Map] launch $Launch -> $Log"
		$Process = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru
		$LastChange = Get-Date
		$LastSize = -1
		while (-not $Process.HasExited) {
			Start-Sleep -Seconds 20
			$Size = 0
			if (Test-Path $Progress) { $Size += (Get-Item $Progress).Length }
			if (Test-Path $Log) { $Size += (Get-Item $Log).Length }
			if ($Size -ne $LastSize) { $LastSize = $Size; $LastChange = Get-Date }
			if (((Get-Date) - $LastChange).TotalMinutes -ge $HangMinutes) {
				Write-Host "[$Map] HANG: no log or progress for $HangMinutes min; killing $($Process.Id)"
				Add-Content $Progress "HANG launch=$Launch no_output_for_${HangMinutes}min"
				Stop-Process -Id $Process.Id -Force
				break
			}
		}
		$Process.WaitForExit()
		$Crashed = (Test-Path $Log) -and (Select-String -Path $Log -Pattern 'Fatal error|Assertion failed|Unhandled Exception|=== Critical error ===' -Quiet)
		$Done = (Test-Path $Progress) -and (Select-String -Path $Progress -Pattern '^RUN_DONE' -Quiet)
		Write-Host "[$Map] launch $Launch exit=$($Process.ExitCode) crashed=$Crashed done=$Done"
		if ($Crashed -or -not $Done) {
			Add-Content $Progress "PROCESS_ENDED launch=$Launch exit=$($Process.ExitCode) crashed=$Crashed log=$Log"
		}
	}
	Write-Host "[$Map] results:"
	if (Test-Path $Progress) { Select-String -Path $Progress -Pattern '^RESULT (FAIL|WARN)|^RUN_DONE|^PROCESS_ENDED|^HANG' | ForEach-Object { $_.Line } }
}
