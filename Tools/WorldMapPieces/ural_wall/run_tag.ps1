# Throwaway headless DebugGame editor on the world campaign level: tags the wall for the anchor keep-clear rule and saves.
param([string]$Root = "D:\UE5Projects\RTS_Survival_DeckTest")
$out = "$Root\Saved\WorldMapWall"
$log = "$out\tag.log"
$exe = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame.exe"
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item $log -ErrorAction SilentlyContinue
$arguments = @("`"$Root\RTS_Survival.uproject`"",
    '-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign',
    '-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False',
    '-ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/WorldMapPieces/ural_wall/tag_wall_keep_clear.py"',
    '-RTSWorldCampaignSaveSlot=UralWallSwap', '-unattended', '-nop4', '-nosound', '-nosplash', '-nullrhi',
    '-NoLiveCoding', '-DisablePlugins=Fab,BpGeneratorUltimate', "-abslog=$log")
$process = Start-Process -PassThru -FilePath $exe -ArgumentList $arguments
if (-not $process.WaitForExit(400000)) { "TIMEOUT"; $process.Kill() }
Select-String -Path $log -Pattern "URALWALL|Traceback" | ForEach-Object { $_.Line.Substring(0, [Math]::Min(220, $_.Line.Length)) }
