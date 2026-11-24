# Runs -ForgeStationValidate headless on Ger_Berlin with the Development editor build
# (code: RTS_Survival/Missions/Berlin/ForgeStation/Tests/ForgeStationTestSubsystem.cpp, compiled where
# RTS_WITH_MAP_TESTS is on). Checks the Forge survey station bonus end to end; see BerlinForgeStation.md.
# Usage: powershell -File run_forge_station_test.ps1 [-Shots] [-DebugGame] [-TimeoutMinutes 12]
param(
    [string]$Map = "/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin",
    [int]$TimeoutMinutes = 12,
    [string]$Log = "",
    # Also save three lit captures (working station, crater side, wrecks) to Saved/ForgeStationTest.
    [switch]$Shots,
    # Use the DebugGame editor build (e.g. while the Development DLL is not rebuilt).
    [switch]$DebugGame
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\ForgeStationTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Log -eq "") { $Log = "$OutDir\validate_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if ($DebugGame) { $Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe" }
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @($Project, $Map, "-game", "-unattended", "-nosound", "-windowed", "-ResX=1280", "-ResY=720",
    "-DisablePlugins=Fab,BpGeneratorUltimate", "-NoLiveCoding", "-ForgeStationValidate", "-abslog=$Log")
if ($Shots) { $Arguments += "-ForgeStationShots" }
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_FORGE_TEST" | ForEach-Object { $_.Line }
