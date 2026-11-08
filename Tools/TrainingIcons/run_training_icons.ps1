# Runs render_training_icons.py in its own DebugGame editor process (never the shared editor).
# Hold Saved/AgentLocks/debuggame_build.lockdir while this runs (the DebugGame DLL is in use).
#   -Mode explore|final   -Jobs "RedSpine,FelsKrieger"   -Map /Game/...   -Ground "x,y"   -View '<json>'
param([string]$Mode = "explore", [string]$Jobs = "", [string]$Map = "/Game/RTS_Survival/Maps/AA_StartGameTutorial/RTS_GameStartTutorial",
      [string]$Ground = "", [string]$View = "", [string]$Overrides = "", [string]$SkyFill = "0", [string]$First = "150", [string]$Second = "240",
      [string]$Log = "D:\UE5Projects\RTS_Survival\Saved\TrainingIcons\render.log")
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot "render_training_icons.py").Replace([char]92, [char]47)
$Done = "D:\UE5Projects\RTS_Survival\Saved\TrainingIcons\renders\done.txt"
if (Test-Path $Done) { Remove-Item $Done }
$env:TRAINICONS_MODE = $Mode
$env:TRAINICONS_JOBS = $Jobs
$env:TRAINICONS_GROUND = $Ground
$env:TRAINICONS_VIEW = $View
$env:TRAINICONS_JOB_OVERRIDES = $Overrides
$env:TRAINICONS_FIRST = $First
$env:TRAINICONS_SKYFILL = $SkyFill
$env:TRAINICONS_SECOND = $Second
$Arguments = @($Project, $Map, "-NoLiveCoding", "-unattended", "-nosplash", "-DisablePlugins=Fab,BpGeneratorUltimate",
    "-ExecCmds=`"py $Script`"", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
"started pid $($Process.Id)"
$Finished = $Process.WaitForExit(1200000)
if (-not $Finished) { "TIMEOUT pid $($Process.Id) still running" }
if (Test-Path $Done) { Get-Content $Done } else { "NO DONE FILE" }
Select-String -Path $Log -Pattern "\[TrainingIcons\]|Error: |LogPython: Error" | Select-Object -First 60 | ForEach-Object { $_.Line }
