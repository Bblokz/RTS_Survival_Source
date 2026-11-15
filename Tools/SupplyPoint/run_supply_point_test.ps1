# Runs -SupplyPointValidate headless on the DebugGame build (code: SupplyPoints/Tests/SupplyPointMapTest.cpp, compiled
# only where RTS_WITH_MAP_TESTS is on). Hold Saved/AgentLocks/debuggame_build.lockdir while it runs.
# Spawns a supply point + a truck base, captures the point with a Pz IV, follows one truck to delivery and destroys
# the next one on its way home. -TruckClass overrides the truck Blueprint of Project Settings > RTS Supply Points.
# -UseHq spawns and deploys a real player HQ as the base (spawn socket, real storage) instead of a scripted base spot.
# -PointClass "/Game/.../BP.BP_C" tests a supply point Blueprint (with its crane); -Pictures saves RTS-camera pictures of
# the bay every 1.5 s into Saved/SupplyPointTest/Pictures.
# Usage: powershell -File run_supply_point_test.ps1 [-TruckClass "/Game/.../BP.BP_C"] [-At "X,Y"] [-UseHq]
param(
    [string]$Map = "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall",
    [string]$TruckClass = "",
    [string]$At = "",
    [switch]$UseHq,
    [switch]$WithSound,
    [string]$PointClass = "",
    [switch]$Pictures,
    [float]$LoadingTime = 0,
    [int]$TimeoutMinutes = 20,
    [string]$Log = ""
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\SupplyPointTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Log -eq "") { $Log = "$OutDir\validate_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @($Project, $Map, "-game", "-unattended", "-windowed", "-ResX=1280", "-ResY=720",
    "-DisablePlugins=Fab", "-NoLiveCoding", "-SupplyPointValidate", "-abslog=$Log")
if ($TruckClass -ne "")
{
    $Arguments += "-ini:Game:[/Script/RTS_Survival.SupplyPointDeveloperSettings]:DefaultSupplyTruckClass=$TruckClass"
}
if ($At -ne "") { $Arguments += "-SupplyPointAt=$At" }
if ($UseHq) { $Arguments += "-SupplyPointUseHq" }
if ($PointClass -ne "") { $Arguments += "-SupplyPointClass=$PointClass" }
if ($Pictures) { $Arguments += "-SupplyPointPictures" }
if ($LoadingTime -gt 0) { $Arguments += "-SupplyPointLoadingTime=$LoadingTime" }
# Without an audio device a voice line never finishes, so the announcer lines pile up and log "line will be lost".
if (-not $WithSound) { $Arguments += "-nosound" }
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_SUPPLYPOINT_TEST" | ForEach-Object { $_.Line }
