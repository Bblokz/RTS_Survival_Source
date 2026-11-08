# Runs round3a_import_icons.py headless on the DebugGame build.
# Hold Saved/AgentLocks/debuggame_build.lockdir AND da_globalabilities.lockdir while this runs.
param([string]$Keys = "", [string]$Log = "D:\UE5Projects\RTS_Survival\Saved\AbilityIconsA\import.log")
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "round3a_import_icons.py").Replace([char]92, [char]47)
$env:ICONSA_KEYS = $Keys
$Arguments = @($Project, "-run=pythonscript", "-script=$Script", "-unattended", "-AllowCommandletRendering",
    "-RenderOffscreen", "-NoLiveCoding", "-DisablePlugins=Fab", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
$Process.WaitForExit(1200000) | Out-Null
Select-String -Path $Log -Pattern "\[IconsAImport\]" | ForEach-Object { $_.Line }
