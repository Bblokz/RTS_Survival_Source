# Runs -Round5FixesValidate headless on the DebugGame build. Hold Saved/AgentLocks/debuggame_build.lockdir.
# Default: RTS_ToCopySmall (templates, Ju 87 click without techs, sFH 18 vs leFH 18 range, Red Spine cargo-door
# walk-out; screenshots Saved/Round5FixesTest). With -Map and -Cap: a mission map's ability-tier cap only.
# Usage: powershell -File run_round5_test.ps1 [-AnchorX -12145 -AnchorY -22000]
#        powershell -File run_round5_test.ps1 -Map /Game/RTS_Survival/Maps/GerMissions/Ger_FirstMission -Cap TierI -Free 1
param(
    [float]$AnchorX = -12145,
    [float]$AnchorY = -22000,
    [string]$Map = "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall",
    [string]$Cap = "",
    [string]$Free = "",
    [string]$Log = ""
)
if ($Log -eq "") { $Log = "D:\UE5Projects\RTS_Survival\Saved\Round5FixesTest\validate_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @($Project, $Map, "-game", "-unattended", "-nosound", "-windowed", "-ResX=1280", "-ResY=720",
    "-DisablePlugins=Fab", "-NoLiveCoding", "-Round5FixesValidate", "-abslog=$Log")
if ($Cap -ne "") { $Arguments += "-Round5MissionCaps=$Cap" }
if ($Free -ne "") { $Arguments += "-Round5MissionFree=$Free" }
else { $Arguments += @("-Round5AnchorX=$AnchorX", "-Round5AnchorY=$AnchorY") }
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit(1200000))
{
    "TIMEOUT: still running after 20 minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_ROUND5_TEST (CHECK|RESULT|RANGES|MISSION_CAP|AIRCRAFT|SQUAD_|JU87|BF109|FELS)" | ForEach-Object { $_.Line }
