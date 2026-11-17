"""Read-only commandlet: T3D-export materials / functions so their graphs can be read as text.

    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
Writes Saved/MapSkirt/Ger_Berlin/t3d/<AssetName>.t3d
"""
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/Ger_Berlin/t3d/'
os.makedirs(OUT, exist_ok=True)
ASSETS = ['/Game/RTS_Survival/Materials/ground/Landscape/MF_RTS_ColorVariation_Landscape',
          '/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_SM_HorizonSkirt',
          '/Game/RTS_Survival/Materials/ground/Landscape/Berlin/MI_RTS_Landscape_Berlin']
for path in ASSETS:
    obj = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = OUT + path.split('/')[-1] + '.t3d'
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    task.exporter = unreal.ObjectExporterT3D()
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log_warning('T3D %s -> %s' % (path, ok))
noise = unreal.load_asset('/Game/RTS_Survival/Textures/Noise/T_RTSNoise_MacroVariation')
unreal.log_warning('NOISE srgb=%s comp=%s size=%dx%d' % (noise.get_editor_property('srgb'), noise.get_editor_property('compression_settings'),
                                                      noise.blueprint_get_size_x(), noise.blueprint_get_size_y()))
for n in ('D', 'N'):
    t = unreal.load_asset('/Game/RTS_Survival/Materials/ground/Landscape/Berlin/Textures/T_Berlin_BerlinRubble_' + n)
    unreal.log_warning('RUBBLE_%s srgb=%s comp=%s size=%dx%d' % (n, t.get_editor_property('srgb'), t.get_editor_property('compression_settings'),
                                                             t.blueprint_get_size_x(), t.blueprint_get_size_y()))
