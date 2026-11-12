# Runs one survival_improvements_test.py pass in a throwaway DebugGame editor on SM_Desert_01 and waits for it.
# Usage: run_survival_test.ps1 -RunJson '{"mode": "picker", "pick": 2, "tag": "picker"}' -Flags '-SurvivalPickerHold'
param(
	[Parameter(Mandatory = $true)][string]$RunJson,
	[string]$Flags = '',
	[int]$TimeoutMinutes = 20,
	[switch]$NoDriveSetup
)
$out = 'D:\UE5Projects\RTS_Survival\Saved\SurvivalImprovements'
New-Item -ItemType Directory -Force $out | Out-Null
$RunJson | Set-Content -Encoding ascii "$out\run.json"
$tag = ($RunJson | ConvertFrom-Json).tag
$exe = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe'
$argList = @(
	'D:/UE5Projects/RTS_Survival/RTS_Survival.uproject',
	'-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01',
	'-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
	$(if ($NoDriveSetup) { '-SurvivalNoop' } else { '-SurvivalDriveSetup' })
) + ($Flags -split ' ' | Where-Object { $_ }) + @(
	'"-ExecCmds=py D:/UE5Projects/RTS_Survival/Source/Tools/SurvivalImprovements/survival_improvements_test.py"',
	'-unattended', '-nosplash', '-DisablePlugins=Fab,BpGeneratorUltimate', '-NoLiveCoding',
	'-ResX=1600', '-ResY=900', '-WINDOWED', "-abslog=$out\$tag.log"
)
$started = Get-Date
$process = Start-Process -FilePath $exe -ArgumentList $argList -PassThru
$deadline = $started.AddMinutes($TimeoutMinutes)
while (-not $process.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 10 }
if (-not $process.HasExited) { "TIMEOUT: editor $($process.Id) still running"; $process.CloseMainWindow() | Out-Null }
"exited after $([int]((Get-Date) - $started).TotalSeconds) s"
Get-ChildItem 'D:\UE5Projects\RTS_Survival\Saved\Screenshots\WindowsEditor' -Filter *.png -ErrorAction SilentlyContinue |
	Where-Object { $_.LastWriteTime -gt $started } | ForEach-Object { "screenshot $($_.FullName)" }
