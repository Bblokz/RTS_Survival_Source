# Runs -SupplyPointValidate on BP_OW_DFStation (capture-only branch of SupplyPoints/Tests/SupplyPointMapTest.cpp):
# the flag must come down and go up, the holder look (EnemyHeld / PlayerHeld parts) and the DFStation minimap icon must
# follow the capture, and no supply cycle or truck may start. -Pictures saves RTS-camera pictures every 1.5 s to
# Saved/SupplyPointTest/Pictures (capture_<n>_owner<o>_flag<progress>.png).
# Uses the Development build (UnrealEditor.exe -game) unless -DebugGame is given; never the shared editor.
param(
    [string]$Map = "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall",
    [string]$At = "16796,-15808",
    [switch]$Pictures,
    [switch]$DebugGame,
    [int]$TimeoutMinutes = 15
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\SupplyPointTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Log = "$OutDir\dfstation_$(Get-Date -Format HHmmss).log"
$Bin = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64"
$Engine = if ($DebugGame) { "$Bin\UnrealEditor-Win64-DebugGame-Cmd.exe" } else { "$Bin\UnrealEditor-Cmd.exe" }
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Class = "/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Stations/BP_OW_DFStation.BP_OW_DFStation_C"
$Arguments = @($Project, $Map, "-game", "-unattended", "-windowed", "-ResX=1280", "-ResY=720", "-nosound",
    "-DisablePlugins=Fab", "-NoLiveCoding", "-SupplyPointValidate", "-SupplyPointAt=$At",
    "-SupplyPointClass=$Class", "-abslog=$Log")
if ($Pictures) { $Arguments += "-SupplyPointPictures" }
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_SUPPLYPOINT_TEST" | ForEach-Object { $_.Line }
