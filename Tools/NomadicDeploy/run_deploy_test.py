"""Runs nomadic_deploy_test.py for one building in a throwaway DebugGame editor and turns the frames into a review
video (<work>/deploy_test.mp4, 15 fps = real speed). One headless run at a time (Saved/NomadicDeploy/headless.lock).
Usage: python run_deploy_test.py <building work dir> [max_seconds]
"""
import glob
import json
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import locks  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__)).replace('\\', '/')
LOCK = 'D:/UE5Projects/RTS_Survival/Saved/NomadicDeploy/headless.lock'
EXE = 'D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe'
ROOT = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/'


def kill_orphaned_web_helpers():
    """Killed / quit editors can leave EpicWebHelper (CEF) children that keep growing until memory runs out."""
    ps = ("$alive = @{}; Get-Process | ForEach-Object { $alive[$_.Id] = 1 }; "
          "Get-CimInstance Win32_Process -Filter \"Name='EpicWebHelper.exe'\" | "
          "Where-Object { -not $alive.ContainsKey([int]$_.ParentProcessId) } | "
          "ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue; $_.ProcessId }")
    out = subprocess.run(['powershell', '-NoProfile', '-Command', ps], capture_output=True, text=True).stdout.split()
    if out:
        print('killed orphaned EpicWebHelper', len(out), flush=True)


def main(argv):
    work = os.path.abspath(argv[1]).replace('\\', '/')
    cfg = json.load(open(work + '/config.json'))
    b = cfg['building']
    frames = work + '/test_frames'
    shutil.rmtree(frames, ignore_errors=True)
    os.makedirs(frames)
    locks.acquire('headless.lock', 'run_deploy_test ' + b, poll=3)
    try:
        json.dump({'work': work, 'deploy_mesh': f'{ROOT}{b}/SM_{b}_Deploy',
                   'max_seconds': float(argv[2]) if len(argv) > 2 else 150.0},
                  open('D:/UE5Projects/RTS_Survival/Saved/NomadicDeploy/test_run.json', 'w'))
        log = f'NomadicTest_{b}.log'
        args = [EXE, 'D:/UE5Projects/RTS_Survival/RTS_Survival.uproject',
                '-ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall',
                '-ini:Engine:[/Script/Engine.Engine]:bUseFixedFrameRate=True',
                '-ini:Engine:[/Script/Engine.Engine]:FixedFrameRate=30',
                f'-ExecCmds=py {HERE}/nomadic_deploy_test.py', '-unattended', '-nosplash',
                '-DisablePlugins=Fab,BpGeneratorUltimate', '-NoLiveCoding', f'-log={log}']
        proc = subprocess.Popen(args)
        try:
            proc.wait(timeout=1500)
        except subprocess.TimeoutExpired:
            proc.kill()
            print('headless editor timed out and was killed')
    finally:
        kill_orphaned_web_helpers()
        locks.release('headless.lock')
    for line in json.load(open(work + '/test_log.json')) if os.path.exists(work + '/test_log.json') else ['no test log']:
        if 't=' not in line or 'stands' in line:
            print(line)
    shots = sorted(glob.glob(frames + '/f_*.png'))
    if not shots:
        raise SystemExit('no frames captured')
    mp4 = work + '/deploy_test.mp4'
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', '15', '-i', frames + '/f_%05d.png',
                    '-vf', f"drawtext=text='{b} nomadic deploy + pack-up':x=12:y=12:fontsize=26:fontcolor=white:box=1:boxcolor=black@0.5",
                    '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '22', mp4], check=True)
    print('wrote', mp4, len(shots), 'frames')


if __name__ == '__main__':
    main(sys.argv)
