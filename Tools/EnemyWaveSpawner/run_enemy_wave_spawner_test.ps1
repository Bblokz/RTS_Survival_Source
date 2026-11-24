# Runs -EnemyWaveSpawnerValidate headless on the DebugGame build
# (code: RTS_Survival/Enemy/EnemyWaves/WaveSpawner/Tests/EnemyWaveSpawnerTestSubsystem.cpp, compiled only where
# RTS_WITH_MAP_TESTS is on). Hold Saved/AgentLocks/debuggame_build.lockdir while this runs (it locks the DebugGame DLL).
# Spawns attack / patrol wave spawners near the camera on UT_EnemyAI and checks wave counts, difficulty unit lists,
# owning-building stops, minimap markers, the direct-control handoff and that nothing shows in game.
# Usage: powershell -File run_enemy_wave_spawner_test.ps1 [-Map /Game/...] [-Log <path>]
param(
    [string]$Map = "/Game/RTS_Survival/Maps/UnitTests/UT_EnemyAI",
    [int]$TimeoutMinutes = 10,
    [string]$Log = ""
)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\EnemyWaveSpawnerTest"
New-Item -ItemType Directory -Force $OutDir | Out-Null
if ($Log -eq "") { $Log = "$OutDir\validate_$(Get-Date -Format HHmmss).log" }
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Arguments = @($Project, $Map, "-game", "-unattended", "-nosound", "-windowed", "-ResX=1280", "-ResY=720",
    "-DisablePlugins=Fab", "-NoLiveCoding", "-EnemyWaveSpawnerValidate", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "RTS_WAVE_SPAWNER_TEST" | ForEach-Object { $_.Line }
