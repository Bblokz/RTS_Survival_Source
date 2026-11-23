# Runs the player economy simulation headless (see Source/Docs/Balance/EconomySim.md).
# Needs an editor build (DebugGame or Development) that contains the RTS_EconomySim module (editor target only).
#   .\run_economy_sim.ps1                       # 30 game minutes on UT_EconomySim
#   .\run_economy_sim.ps1 -Minutes 5            # short smoke run
#   .\run_economy_sim.ps1 -Extra '-EconSimNoProtect -EconSimStart=1000,400,0'
param(
	[float]$Minutes = 30,
	[float]$Dilation = 1,
	[string]$Configuration = "DebugGame",
	[string]$Extra = "",
	[string]$OutputDirectory = ""
)

$Engine = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
$Exe = if ($Configuration -eq 'Development') { "$Engine\UnrealEditor-Cmd.exe" } else { "$Engine\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
# The Shorts worktree opens its own .uproject copy; the main checkout has only RTS_Survival.uproject.
$Project = Join-Path $ProjectRoot 'RTS_SurvivalShorts.uproject'
if (-not (Test-Path $Project)) { $Project = Join-Path $ProjectRoot 'RTS_Survival.uproject' }
if ($OutputDirectory -eq "") {
	$OutputDirectory = Join-Path $ProjectRoot ('Saved\EconomySim\' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$Log = Join-Path $OutputDirectory 'sim.log'

# Launch from PowerShell: Git Bash rewrites /Game/... paths.
$Arguments = @($Project, '/Game/RTS_Survival/Maps/UnitTests/UT_EconomySim', '-game', '-unattended', '-nop4', '-nosound',
	# Not -nullrhi: the enemy's fog of war vision needs a renderer.
	'-RenderOffscreen', '-windowed', '-ResX=640', '-ResY=360', '-DisablePlugins=Fab,BpGeneratorUltimate', '-NoLiveCoding',
	'-EconSim', "-EconSimMinutes=$Minutes", "-EconSimDilation=$Dilation", "-EconSimDir=$OutputDirectory",
	"-abslog=$Log", '-ExecCmds="Module Load RTS_EconomySim"')
if ($Extra -ne "") { $Arguments += $Extra.Split(' ') }

$Process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
$Process.WaitForExit()
Select-String -Path $Log -Pattern 'ECONSIM (RESULT|.* step_done|.* finish|.* warning)' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('ECONSIM')) }
Write-Output "Output: $OutputDirectory"
