# Runs larch_test.py in a throwaway DebugGame editor on RTS_ToCopySmall (never the shared editor). Kills only its own editor on timeout.
param([string]$Name = 'larch1', [int]$Limit = 640)
$root = 'D:\UE5Projects\RTS_Survival\Saved\WinterLarch\test'
New-Item -ItemType Directory -Force $root | Out-Null
$log = "$root\$Name.log"
$exe = 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe'
$argList = '"D:\UE5Projects\RTS_Survival\RTS_Survival.uproject" "-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall" -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WinterLarch/larch_test.py" -LogCmds="LogWreckHoleDamage Verbose" -ResX=1280 -ResY=720 -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog="' + $log + '"'
$p = Start-Process $exe -ArgumentList $argList -PassThru
if (-not $p.WaitForExit($Limit * 1000)) { 'timeout, killing own editor'; Stop-Process -Id $p.Id -Force }
"exit code $($p.ExitCode)"
Select-String -Path $log -Pattern 'LARCHTEST|WreckHole.*(Error|Warning)|Script Stack|Fatal' | Select-Object -Last 90 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(600, $_.Line.Length)) }
