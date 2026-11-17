# Offscreen check of the deck builder card menu (UDeckBuilderCaptureSubsystem, -DeckBuilderCapture).
# Starts the world campaign map as a game on a seeded new campaign, steps through the menu and writes
# Saved/DeckBuilderCapture/deckbuilder_<step>.png plus the deck after each step to the log.
#
#   -Build DebugGame   : UnrealEditor-Win64-DebugGame-Cmd.exe. Hold Saved/AgentLocks/debuggame_build.lockdir.
#   -Build Development : UnrealEditor-Cmd.exe (the DLL the shared editor pins; reading it is fine).
#   -Tests             : runs the RTS.DeckBuilder automation tests under -nullrhi instead of the capture.
#
# Starting the campaign map on a new campaign saves that campaign, and the capture's last step presses
# Save & Go Back, so the player's campaign save (Saved/SaveGames/WorldCampaignState.sav and its backup) is copied
# aside first and put back afterwards; when there was none, the capture's own save is removed again.
#   -Root <folder>     : another project folder to run, e.g. an isolated copy with its own Binaries and Saved for
#                        when the shared editor DLLs are locked. Defaults to this checkout.
param([string]$Build = "DebugGame", [switch]$Tests, [string]$Root = "")
if ($Root -eq "") { $Root = (Resolve-Path "$PSScriptRoot\..\..\..").Path }
$Root = $Root.Replace('/', '\')
$Bin = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64"
$Engine = if ($Build -eq "DebugGame") { "$Bin\UnrealEditor-Win64-DebugGame-Cmd.exe" } else { "$Bin\UnrealEditor-Cmd.exe" }
$Project = "$Root\RTS_Survival.uproject"
$Out = "$Root\Saved\DeckBuilderCapture"
New-Item -ItemType Directory -Force $Out | Out-Null

if ($Tests) {
    $Log = "$Out\tests_$Build.log"
    $Arguments = @($Project, "-nullrhi", "-unattended", "-nop4", "-nosplash", "-nosound", "-NoLiveCoding",
        "-DisablePlugins=Fab,BpGeneratorUltimate", "-ini:Game:[StartupActions]:bAddPacks=False",
        "`"-ExecCmds=Automation RunTests RTS.DeckBuilder`"", "`"-TestExit=Automation Test Queue Empty`"", "-abslog=$Log")
    $Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (-not $Process.WaitForExit(900000)) { "TIMEOUT pid $($Process.Id)"; $Process.Kill() }
    "EXIT $($Process.ExitCode)"
    Select-String -Path $Log -Pattern "Test Completed|DeckBuilder.*(Error|Expected)|LogAutomationController: (Error|Warning)|tests? (passed|failed)" |
        Select-Object -Last 40 | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }
    return
}

$Log = "$Out\capture_$Build.log"
$SaveGames = "$Root\Saved\SaveGames"
$SaveBackup = "$Out\campaign_save_before_capture"
Remove-Item -Recurse -Force $SaveBackup -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$SaveBackup\backup" | Out-Null
Copy-Item "$SaveGames\WorldCampaignState.sav" $SaveBackup -ErrorAction SilentlyContinue
Copy-Item "$SaveGames\backup\WorldCampaignState.sav" "$SaveBackup\backup" -ErrorAction SilentlyContinue
Remove-Item "$Out\deckbuilder_*.png" -ErrorAction SilentlyContinue

$Arguments = @($Project, "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign", "-game", "-unattended", "-nop4",
    "-nosound", "-RenderOffscreen", "-windowed", "-ResX=1920", "-ResY=1080", "-NoLiveCoding",
    "-DisablePlugins=Fab,BpGeneratorUltimate", "-ini:Game:[StartupActions]:bAddPacks=False", "-DeckBuilderCapture",
    "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -WindowStyle Hidden -PassThru
if (-not $Process.WaitForExit(600000)) { "TIMEOUT pid $($Process.Id)"; $Process.Kill() }
"EXIT $($Process.ExitCode)"

if (Test-Path "$SaveBackup\WorldCampaignState.sav") {
    Copy-Item "$SaveBackup\WorldCampaignState.sav" $SaveGames -Force
    Copy-Item "$SaveBackup\backup\WorldCampaignState.sav" "$SaveGames\backup" -Force -ErrorAction SilentlyContinue
    "campaign save restored"
} else {
    Remove-Item "$SaveGames\WorldCampaignState.sav" -ErrorAction SilentlyContinue
    Remove-Item "$SaveGamesackup\WorldCampaignState.sav" -ErrorAction SilentlyContinue
    "capture's campaign save removed (there was none before)"
}
Select-String -Path $Log -Pattern "DECKCAPTURE|DECKBUILDER|Error: .*(Deck|deck)" |
    Select-Object -Last 90 | ForEach-Object { $_.Line -replace '^\[[^\]]*\]\[[^\]]*\]', '' }
Get-ChildItem "$Out\deckbuilder_*.png" | ForEach-Object { $_.Name }
