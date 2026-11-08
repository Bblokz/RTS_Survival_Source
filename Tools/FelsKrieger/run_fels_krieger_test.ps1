# Runs -FelsKriegerValidate headless on the DebugGame build (spawned squad: subtype, health, Ripper Guns, AnimBP and
# pose motion while idle/walking/firing, move + attack-ground; then GA_GerFelsKriegerDrop lands 2 squads).
# Hold Saved/AgentLocks/debuggame_build.lockdir. Screenshots: Saved/FelsKriegerTest.
# Usage: powershell -File run_fels_krieger_test.ps1 [-AnchorX -12145 -AnchorY -22000]
param(
    [float]$AnchorX = -12145,
    [float]$AnchorY = -22000,
    [string]$Log = "$PSScriptRoot\..\..\..\Saved\Logs\FelsKriegerValidate.log"
)
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @(
    $Project, "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall", "-game", "-unattended", "-nosound",
    "-windowed", "-ResX=1280", "-ResY=720", "-DisablePlugins=Fab", "-NoLiveCoding",
    "-FelsKriegerValidate", "-FelsKriegerAnchorX=$AnchorX", "-FelsKriegerAnchorY=$AnchorY",
    "-abslog=$Log"
)
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit(900000))
{
    "TIMEOUT: the run is still going after 15 minutes (pid $($Process.Id)); check the log"
}
Select-String -Path $Log -Pattern "RTS_FELSKRIEGER_TEST (CHECK|RESULT|ANIM|POSE)" | ForEach-Object { $_.Line }
