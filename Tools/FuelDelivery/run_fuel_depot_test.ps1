# Runs -FuelDepotValidate headless on the DebugGame build (code: Missions/MissionProps/FuelDelivery/Tests/
# FuelDepotTestSubsystem.cpp, only where RTS_WITH_MAP_TESTS is on). Hold Saved/AgentLocks/debuggame_build.lockdir
# while it runs. Spawns Rus_BXP_LongCamoBunker_FuelDepot for player 2 and follows three trucks (see
# Source/Docs/Missions/FuelDeliveryDepot.md). -Capture renders (no -nullrhi) and saves pictures of the parked truck.
# Usage: powershell -File run_fuel_depot_test.ps1 [-At "X,Y"] [-Yaw 0] [-Class "/Game/.../BP.BP_C"] [-Capture]
param(
    [string]$Map = "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall",
    [string]$At = "",
    [float]$Yaw = 0,
    [string]$Class = "",
    [switch]$Capture,
    [int]$TimeoutMinutes = 25,
    [string]$Log = ""
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\FuelDepotTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Log -eq "") { $Log = "$OutDir\validate_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @($Project, $Map, "-game", "-unattended", "-nosound", "-DisablePlugins=Fab", "-NoLiveCoding",
    "-FuelDepotValidate", "-FuelDepotYaw=$Yaw", "-abslog=$Log")
if ($Capture) { $Arguments += @("-windowed", "-ResX=1280", "-ResY=720", "-FuelDepotCapture") }
else { $Arguments += "-nullrhi" }
if ($At -ne "") { $Arguments += "-FuelDepotAt=$At" }
if ($Class -ne "") { $Arguments += "-FuelDepotClass=$Class" }
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_FUELDEPOT" | ForEach-Object { $_.Line }
