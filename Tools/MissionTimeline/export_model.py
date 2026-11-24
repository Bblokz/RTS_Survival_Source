"""Export a mission map's timeline model with the MissionTimelineExport commandlet (headless, no GPU, no play test).

Usage:
    python export_model.py <MapName or /Game/... path> [--config DebugGame|Development]

Writes Saved/MissionTimeline/<MapName>/model.json (and export.log next to it). The commandlet lives in the game module
(Missions/Timeline/MissionTimelineExportCommandlet), so the chosen editor configuration must be built with it. Pick the
one no open editor holds: a running editor locks its DLL, not the other configuration's.
"""
import argparse
import os
import subprocess
import sys

ENGINE = r'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64'
PROJECT = r'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject'
SAVED = r'D:\UE5Projects\RTS_Survival\Saved\MissionTimeline'
MAP_FOLDER = '/Game/RTS_Survival/Maps/GerMissions/'
EXECUTABLES = {'DebugGame': 'UnrealEditor-Win64-DebugGame-Cmd.exe', 'Development': 'UnrealEditor-Cmd.exe'}


def map_path(name):
    return name if name.startswith('/Game/') else MAP_FOLDER + name


def out_folder(map_name):
    folder = os.path.join(SAVED, map_name.rsplit('/', 1)[-1])
    os.makedirs(folder, exist_ok=True)
    return folder


def export(map_name, config='DebugGame'):
    folder = out_folder(map_name)
    model = os.path.join(folder, 'model.json')
    log = os.path.join(folder, 'export.log')
    command = [os.path.join(ENGINE, EXECUTABLES[config]), PROJECT, '-run=MissionTimelineExport',
               '-Map=' + map_path(map_name), '-Out=' + model, '-unattended', '-nullrhi', '-nosplash', '-NoSound',
               '-NoLiveCoding', '-DisablePlugins=Fab,BpGeneratorUltimate', '-abslog=' + log]
    result = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(log, encoding='utf-8', errors='replace') as handle:
        lines = [line.rstrip() for line in handle if 'MissionTimelineExport' in line]
    for line in lines[-5:]:
        print(line)
    if result.returncode != 0 or not os.path.exists(model):
        sys.exit(f'export failed (exit {result.returncode}); see {log}')
    print(f'model: {model} ({os.path.getsize(model) // 1024} KB)')
    return model


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('map')
    parser.add_argument('--config', default='DebugGame', choices=sorted(EXECUTABLES))
    arguments = parser.parse_args()
    export(arguments.map, arguments.config)
