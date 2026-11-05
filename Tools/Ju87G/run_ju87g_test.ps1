# Runs -Ju87GTankBustersValidate headless on the DebugGame build (close-up of one Ju 87 G, then the
# GA_GerJu87GTankBusters strike over 2 T-34/76 + 1 KV-1E). Hold Saved/AgentLocks/debuggame_build.lockdir.
# Screenshots: Saved/Ju87GTest. Usage: powershell -File run_ju87g_test.ps1 [-AnchorX -12145 -AnchorY -22000]
param(
    [float]$AnchorX = -12145,
    [float]$AnchorY = -22000,
    [string]$Log = "$PSScriptRoot\..\..\..\Saved\Logs\Ju87GTankBustersValidate.log"
)
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @(
    $Project, "/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall", "-game", "-unattended", "-nosound",
    "-windowed", "-ResX=1280", "-ResY=720", "-DisablePlugins=Fab", "-NoLiveCoding",
    "-Ju87GTankBustersValidate", "-Ju87GAnchorX=$AnchorX", "-Ju87GAnchorY=$AnchorY",
    "-abslog=$Log"
)
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
$Process.WaitForExit(900000) | Out-Null
Select-String -Path $Log -Pattern "RTS_JU87G_TEST (CHECK|RESULT)" | ForEach-Object { $_.Line }
