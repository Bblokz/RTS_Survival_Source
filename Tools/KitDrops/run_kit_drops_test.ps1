# Runs -KitDropsValidate headless on the DebugGame build (code: GlobalAbilitySystem/Tests/KitDrops/KitDropsTestRunner.cpp,
# compiled only where RTS_WITH_MAP_TESTS is on, never into plain Shipping). Hold Saved/AgentLocks/debuggame_build.lockdir.
# Fires the 13 commander kit abilities no other test fires (glider drops, landed Ju 390 drops, paradrops, Bf 109) one
# after another; screenshots + result_<time>.txt in Saved/KitDropsTest.
# Usage: powershell -File run_kit_drops_test.ps1 [-Only "GA_GerE50Drop+GA_Bf109Strafing"] [-TimeScale 2]
param(
    [string]$Map = "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall",
    [string]$Only = "",
    [float]$TimeScale = 1,
    [int]$TimeoutMinutes = 75,
    [string]$Log = ""
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\KitDropsTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Log -eq "") { $Log = "$OutDir\validate_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @($Project, $Map, "-game", "-unattended", "-nosound", "-windowed", "-ResX=1280", "-ResY=720",
    "-DisablePlugins=Fab", "-NoLiveCoding", "-KitDropsValidate", "-KitDropsTimeScale=$TimeScale", "-abslog=$Log")
if ($Only -ne "") { $Arguments += "-KitDropsOnly=$Only" }
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_KIT_DROPS_TEST (CHECK FAIL|CASE_RESULT|RESULT|START|NOTE)" | ForEach-Object { $_.Line }
