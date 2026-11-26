# Films the tactical map for the Direction Finding field manual: -DirectionFindingClip on Ger_OderWarthe
# (Missions/DirectionFinding/Tests/DirectionFindingClipCapture.cpp). One screenshot with the UI per frame at a fixed
# 24 fps lands in Saved/DirectionFindingClip/Frames; DF_CLIP lines in the log give the frame of every event.
# Command post forced to Werk 2 (index 1): north of DF 1 and north-west of DF 2, so the wedges cross clearly.
# Check nvidia-smi first; never run it in the shared editor. DebugGame build (the -Cmd binary renders the UI).
param([int]$TimeoutMinutes = 15)
$OutDir = "D:\UE5Projects\RTS_Survival\Saved\DirectionFindingClip"
Remove-Item -Recurse -Force "$OutDir\Frames" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$OutDir\Frames" | Out-Null
$Log = "$OutDir\clip.log"
$Engine = "D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
$Arguments = @("D:\UE5Projects\RTS_Survival\RTS_Survival.uproject", "/Game/RTS_Survival/Maps/GerMissions/Ger_OderWarthe",
    "-game", "-unattended", "-windowed", "-ResX=1920", "-ResY=1080", "-nosound", "-benchmark", "-fps=24",
    "-DisablePlugins=Fab", "-NoLiveCoding", "-DirectionFindingClip", "-DirectionFindingCommandPost=1",
    "-abslog=$Log")
$Process = Start-Process -FilePath $Engine -ArgumentList $Arguments -PassThru
if (-not $Process.WaitForExit($TimeoutMinutes * 60000))
{
    "TIMEOUT: still running after $TimeoutMinutes minutes (pid $($Process.Id)); not killed, check the log"
}
"LOG $Log"
Select-String -Path $Log -Pattern "DF_CLIP" | ForEach-Object { $_.Line }
"frames: " + (Get-ChildItem "$OutDir\Frames" -Filter *.png).Count
