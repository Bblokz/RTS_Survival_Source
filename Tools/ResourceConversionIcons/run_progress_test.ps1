# Runs conversion_progress_headless_test.py in a throwaway DebugGame editor (not the shared one, no UECP).
param([string]$Map = "/Game/RTS_Survival/Maps/GerMissions/Ger_DestroyT35",
      [string]$Script = "conversion_progress_headless_test.py",
      [string]$Log = "$PSScriptRoot\..\..\..\Saved\ResourceConversionIcons\progress_test.log")
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
$Project = "D:\UE5Projects\RTS_Survival\RTS_Survival.uproject"
$Script = (Join-Path $PSScriptRoot $Script).Replace([char]92, [char]47)
$Arguments = @($Project, "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=$Map",
    "-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False",
    "-ExecCmds=`"py $Script`"", "-unattended", "-nosplash", "-DisablePlugins=Fab,BpGeneratorUltimate",
    "-NoLiveCoding", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
$Finished = $Process.WaitForExit(900000)
if (-not $Finished) { Stop-Process -Id $Process.Id -Force; "TIMEOUT pid $($Process.Id)" }
"EXIT $($Process.ExitCode)"
Select-String -Path $Log -Pattern "CONVPROG|Error: .*Python|ConversionProgressBar" | Select-Object -Last 40 | ForEach-Object { $_.Line }
