"""Imports T_RunOver_ClipAtlas, the run-over clip on page 2 of the crushing field manual (UW_WallBreachTakeover), into
GameUI/InfoTakeover/WallBreach as a UI texture without mips that is never streamed (the widget shows every cell of it).

The atlas comes from ../VulturesAt/make_clip_atlas.py --clip RunOver (Saved/InfoTakeover/RunOver/T_RunOver_ClipAtlas.png
+ clip_atlas.json). The grid is NOT written into WBP_WallBreachTakeover: UW_WallBreachTakeover's constructor holds it
(RunOverClipColumns / Rows / FrameCount), so check clip_atlas.json against those constants; this script logs both.

UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
-DisablePlugins=Fab,BpGeneratorUltimate
"""
import json
import os

import unreal

TARGET_FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WallBreach'
TEXTURE_NAME = 'T_RunOver_ClipAtlas'
SAVED = os.path.join(unreal.Paths.project_saved_dir(), 'InfoTakeover', 'RunOver')
TEXTURE_SOURCE = os.path.join(SAVED, TEXTURE_NAME + '.png')
ATLAS_INFO = os.path.join(SAVED, 'clip_atlas.json')


def log(message):
    unreal.log('RUN_OVER_ASSET ' + message)


def import_texture():
    if not os.path.isfile(TEXTURE_SOURCE):
        raise RuntimeError('RUN_OVER_ASSET missing atlas ' + TEXTURE_SOURCE)
    task = unreal.AssetImportTask()
    task.filename = TEXTURE_SOURCE
    task.destination_path = TARGET_FOLDER
    task.destination_name = TEXTURE_NAME
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(TARGET_FOLDER + '/' + TEXTURE_NAME)
    if texture is None:
        raise RuntimeError('RUN_OVER_ASSET texture import failed')
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('srgb', True)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    saved = unreal.EditorAssetLibrary.save_asset(TARGET_FOLDER + '/' + TEXTURE_NAME, False)
    log('texture %dx%d saved: %s' % (texture.blueprint_get_size_x(), texture.blueprint_get_size_y(), saved))
    with open(ATLAS_INFO) as handle:
        atlas = json.load(handle)
    cdo = unreal.get_default_object(unreal.W_WallBreachTakeover)
    clip = cdo.get_editor_property('m_run_over_clip')
    grid = (clip.get_editor_property('columns'), clip.get_editor_property('rows'),
            clip.get_editor_property('frame_count'))
    log('atlas grid %s x %s, %s frames; C++ grid %s x %s, %s frames; match %s' % (
        atlas['columns'], atlas['rows'], atlas['frames'], grid[0], grid[1], grid[2],
        (atlas['columns'], atlas['rows'], atlas['frames']) == grid))


import_texture()
