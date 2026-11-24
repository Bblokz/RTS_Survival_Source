# Offscreen pictures of the world-map country look (UWorldCountryLookCaptureSubsystem, -WorldCountryLookCapture).
# Generates a seeded campaign on its own save slot, clears targets step by step and writes
# Saved/WorldCountryLookCapture/countrylook_<step>.png. The player's campaign save is not touched.
# Check nvidia-smi first: an offscreen render next to a busy editor can exhaust the GPU.
#   -Root <folder> : another project folder to run, e.g. the isolated copy with its own Binaries and Saved.
param([string]$Build = "DebugGame", [string]$Root = "D:\UE5Projects\RTS_Survival")

$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-$Build.exe"
$Out = "$Root\Saved\WorldCountryLookCapture"
$Log = "$Out\capture_$Build.log"
$Slot = "WorldCountryLookCapture"
New-Item -ItemType Directory -Force $Out | Out-Null
Remove-Item "$Out\countrylook_*.png" -ErrorAction SilentlyContinue

$Arguments = @("`"$Root\RTS_Survival.uproject`"", "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign", "-game", "-unattended",
    "-nop4", "-nosound", "-RenderOffscreen", "-windowed", "-ResX=1920", "-ResY=1080", "-NoLiveCoding",
    "-DisablePlugins=Fab,BpGeneratorUltimate", "-ini:Game:[StartupActions]:bAddPacks=False", "-WorldCountryLookCapture",
    "-RTSWorldCampaignSaveSlot=$Slot", "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -WindowStyle Hidden -PassThru
if (-not $Process.WaitForExit(540000)) { "TIMEOUT pid $($Process.Id)"; $Process.Kill() }
"EXIT $($Process.ExitCode)"

Remove-Item "$Root\Saved\SaveGames\$Slot.sav" -ErrorAction SilentlyContinue
Remove-Item "$Root\Saved\SaveGames\backup\$Slot.sav" -ErrorAction SilentlyContinue
Select-String -Path $Log -Pattern "COUNTRYLOOK" | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }
Get-ChildItem "$Out\countrylook_*.png" | Select-Object Name, Length
