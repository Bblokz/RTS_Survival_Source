# Runs -RailTrooperAnimValidate headless on the DebugGame build: a spawned Squad_Ger_RailTroopers idles, walks and fires
# (mesh/skeleton/AnimBP/M920-in-hand checks and bone motion vs the reference pose), then GA_GerRailTroopersParadrop lands
# more troopers and the same checks run on them. Hold Saved/AgentLocks/debuggame_build.lockdir.
# Screenshots: Saved/RailTrooperSkeletonTest.
# Usage: powershell -File run_rail_trooper_anim_test.ps1 [-AnchorX -12145 -AnchorY -22000]
param(
    [float]$AnchorX = -12145,
    [float]$AnchorY = -22000,
    [string]$Log = "$PSScriptRoot\..\..\..\Saved\Logs\RailTrooperAnimValidate.log"
)
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @(
    $Project, "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall", "-game", "-unattended", "-nosound",
    "-windowed", "-ResX=1280", "-ResY=720", "-DisablePlugins=Fab,BpGeneratorUltimate", "-NoLiveCoding",
    "-RailTrooperAnimValidate", "-RailTrooperAnchorX=$AnchorX", "-RailTrooperAnchorY=$AnchorY",
    "-abslog=$Log"
)
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit(900000))
{
    "TIMEOUT: the run is still going after 15 minutes (pid $($Process.Id)); check the log"
}
Select-String -Path $Log -Pattern "RTS_RAILTROOPER_TEST (CHECK|RESULT|ANIM|POSE|PARADROP|DROP)" | ForEach-Object { $_.Line }
