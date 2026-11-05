# Runs -FireStrikesValidate headless on the DebugGame build (GA_GerIncendiaryJu87 + GA_GerFireBarrage).
# Hold Saved/AgentLocks/debuggame_build.lockdir while this runs (it locks the DebugGame DLL).
# Usage: powershell -File run_fire_strikes_test.ps1 [-AnchorX -12145 -AnchorY -22000] [-Log <path>]
param(
    [float]$AnchorX = -12145,
    [float]$AnchorY = -22000,
    [string]$Log = "$PSScriptRoot\..\..\..\Saved\Logs\FireStrikesValidate.log"
)
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @(
    $Project, "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall", "-game", "-unattended", "-nosound",
    "-windowed", "-ResX=1280", "-ResY=720", "-DisablePlugins=Fab", "-NoLiveCoding",
    "-FireStrikesValidate", "-FireStrikesAnchorX=$AnchorX", "-FireStrikesAnchorY=$AnchorY",
    "-abslog=$Log"
)
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
$Process.WaitForExit(900000) | Out-Null
Select-String -Path $Log -Pattern "RTS_FIRE_TEST (CHECK|RESULT)" | ForEach-Object { $_.Line }
