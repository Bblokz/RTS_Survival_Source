# Runs -SovietAirSupplyValidate headless on the DebugGame build
# (code: RTS_Survival/Enemy/AirSupply/Tests/SovietAirSupplyTestSubsystem.cpp, only where RTS_WITH_MAP_TESTS is on).
# Spawns BP_SovietAirSupplyPad near the camera on UT_EnemyAI: full delivery, shot down while unloading, shot down on
# approach. Windowed (not nullrhi) so the screenshots in Saved/SovietAirSupplyTest render.
# Usage: powershell -File run_air_supply_test.ps1 [-Map /Game/...] [-Log <path>]
param(
    [string]$Map = "/Game/RTS_Survival/Maps/UnitTests/UT_EnemyAI",
    [int]$TimeoutMinutes = 14,
    [string]$Log = ""
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\SovietAirSupplyTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Log -eq "") { $Log = "$OutDir\validate_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @($Project, $Map, "-game", "-unattended", "-nosound", "-windowed", "-ResX=1280", "-ResY=720",
    "-DisablePlugins=Fab", "-NoLiveCoding", "-SovietAirSupplyValidate", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); killed"
    Stop-Process -Id $Process.Id -Force
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_AIRSUPPLY_TEST|AIR_SUPPLY" | ForEach-Object { $_.Line }
