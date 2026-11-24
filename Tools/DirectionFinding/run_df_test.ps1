# Runs -DirectionFindingValidate on Ger_OderWarthe (Missions/DirectionFinding/Tests/DirectionFindingMapTest.cpp): a
# solver sweep over every command post and station set, then the placed ADirectionFindingManager through capture,
# bursts, reply cooldown, overlap, a lost and retaken station and the fix with the targets revealed.
# -Pictures saves screenshots with the UI (minimap wedges) to Saved/Screenshots/WindowsEditor (check nvidia-smi first).
# Uses the Development build (UnrealEditor-Cmd.exe -game) unless -DebugGame is given; never the shared editor.
param(
    [switch]$Pictures,
    [switch]$DebugGame,
    [int]$TimeoutMinutes = 15
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\DirectionFindingTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Log = "$OutDir\df_$(Get-Date -Format HHmmss).log"
$Bin = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64"
$Engine = if ($DebugGame) { "$Bin\UnrealEditor-Win64-DebugGame-Cmd.exe" } else { "$Bin\UnrealEditor-Cmd.exe" }
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Map = "/Game/RTS_Survival/Maps/GerMissions/Ger_OderWarthe"
$Arguments = @($Project, $Map, "-game", "-unattended", "-windowed", "-ResX=1280", "-ResY=720", "-nosound",
    "-DisablePlugins=Fab", "-NoLiveCoding", "-DirectionFindingValidate", "-abslog=$Log")
if ($Pictures) { $Arguments += "-DirectionFindingPictures" }
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_DF_TEST|Direction-finding manager" | ForEach-Object { $_.Line }
