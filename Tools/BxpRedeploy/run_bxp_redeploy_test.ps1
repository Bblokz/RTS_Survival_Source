# Runs -BxpPackUpRedeployValidate headless on the DebugGame build
# (code: Buildings/BuildingExpansion/Tests/BxpPackUpRedeployTestSubsystem.cpp, map tests only, never in Shipping).
# A German HQ deploys at yaw 90 with slot 0 empty, a Socket BXP in slot 1 and an AtBuildingOrigin BXP in slot 2, packs up
# and redeploys at yaw 180; checks both BXPs come back in their own slots, on the same socket / origin and rotation.
# Hold Saved/AgentLocks/debuggame_build.lockdir while this runs (it locks the DebugGame DLL).
# Usage: powershell -File run_bxp_redeploy_test.ps1 [-AnchorX -2642 -AnchorY -4082] [-Log <path>]
param(
    [float]$AnchorX = -2642,
    [float]$AnchorY = -4082,
    [int]$TimeoutMinutes = 10,
    [string]$Log = ""
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\BxpRedeployTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Log -eq "") { $Log = "$OutDir\validate_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @(
    $Project, "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall", "-game", "-unattended", "-nosound",
    "-windowed", "-ResX=1280", "-ResY=720", "-DisablePlugins=Fab,BpGeneratorUltimate", "-NoLiveCoding",
    "-BxpPackUpRedeployValidate", "-BxpRedeployAnchorX=$AnchorX", "-BxpRedeployAnchorY=$AnchorY",
    "-abslog=$Log"
)
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_BXP_REDEPLOY_TEST (CHECK|RESULT|FIRST_PLACEMENT|ORIGIN_ROTATION|slot=)" |
    ForEach-Object { $_.Line }
