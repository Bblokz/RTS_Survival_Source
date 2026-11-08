# Packages the RTS_SurvivalShippingGlobalAbilityTests target (every global ability unlocked, test runner compiled in)
# as a staged Shipping build. Usage:
#   powershell -File package_global_ability_tests.ps1 [-StageDir E:\RR_test\FInalWindows] [-SkipCook] [-CodeOnly]
# -CodeOnly rebuilds the game exe and copies it (with its PDB) into an existing stage; use it when only C++ changed.
# -SkipCook re-stages already cooked content (the cook can hang after "Packages Remain 0").
param(
	[string]$StageDir = 'E:\RR_test\FInalWindows',
	[switch]$SkipCook,
	[switch]$CodeOnly,
	# Incremental cook: only changed packages. Fast for the fix loop; do a full cook for the final package.
	[switch]$Iterate,
	[string]$LogDir = 'E:\RR_test\GA_logs'
)

$ErrorActionPreference = 'Stop'
$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine'
$Project = 'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
$Target = 'RTS_SurvivalShippingGlobalAbilityTests'
New-Item -ItemType Directory -Force $LogDir | Out-Null
$Stamp = Get-Date -Format 'yyyyMMdd_HHmmss'

# The game exe is built here, not by BuildCookRun -build: that also builds the editor, which fails while an open
# editor has Live Coding on.
$BuildLog = Join-Path $LogDir "build_$Stamp.log"
& "$Engine\Engine\Build\BatchFiles\Build.bat" $Target Win64 Shipping "-Project=$Project" -WaitMutex -NoHotReload *> $BuildLog
# Build.bat can exit 0 after compile errors; the log is the truth.
if ($LASTEXITCODE -ne 0 -or (Select-String -Path $BuildLog -Pattern ' error [A-Z]+[0-9]+:|BUILD FAILED|Result: Failed' -Quiet)) {
	throw "Build failed, see $BuildLog"
}

if ($CodeOnly) {
	$Bin = 'D:\UE5Projects\RTS_Survival\Binaries\Win64'
	$Dest = Join-Path $StageDir 'Windows\RTS_Survival\Binaries\Win64'
	if (-not (Test-Path $Dest)) { throw "No staged package at $StageDir; run without -CodeOnly first." }
	foreach ($Ext in 'exe', 'pdb') {
		Copy-Item -LiteralPath "$Bin\$Target-Win64-Shipping.$Ext" -Destination "$Dest\$Target-Win64-Shipping.$Ext" -Force
	}
	Write-Host "Copied the rebuilt exe into $Dest"
	exit 0
}

# The menu, the campaign map, every mission, the tutorials, the survival maps and two test maps (baseline sweep).
$Maps = @(
	'/Game/RTS_Survival/Maps/ShaderCompilation/RTS_ShaderCompilation',
	'/Game/RTS_Survival/Blueprints/GameUI/StartGame/StartGame',
	'/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign',
	'/Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission',
	'/Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission',
	'/Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35',
	'/Game/RTS_Survival/Maps/GerMissions/Ger_Trains',
	'/Game/RTS_Survival/Maps/GerMissions/RedLine',
	'/Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_GameStartTutorial',
	'/Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_MechanicsTutorial',
	'/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01',
	'/Game/RTS_Survival/SurvivalMode/Maps/SM_Kessel_01',
	'/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall',
	'/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics'
) -join '+'

$CookArg = if ($SkipCook) { '-skipcook' } else { '-cook' }
$IterateArg = if ($Iterate -and -not $SkipCook) { '-iterate' } else { '-nop4' }
$UatLog = Join-Path $LogDir "package_$Stamp.log"
Write-Host "Packaging $Target to $StageDir (log $UatLog)"
& "$Engine\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Project" -noP4 -platform=Win64 `
	"-target=$Target" -clientconfig=Shipping -skipbuild $CookArg $IterateArg -stage -pak -iostore -compressed -prereqs `
	"-stagingdirectory=$StageDir" "-map=$Maps" -nocompileeditor -unattended -utf8output *> $UatLog
$Code = $LASTEXITCODE
Write-Host "BuildCookRun exit $Code"
exit $Code
