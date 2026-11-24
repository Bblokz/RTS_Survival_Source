"""Creates the Wespe Barrage takeover assets in GameUI/InfoTakeover/WespeBarrage:
- T_WespeBarrage_ClipAtlas: the clip flipbook from make_clip_atlas.py, imported as a UI texture without mips that is
  never streamed (the widget shows every cell of it, so it must stay sharp and resident);
- WBP_WespeBarrageTakeover: a copy of WBP_SpacedArmourTakeover (already on the "Rotsignal" M_Style skin, Proto text
  styles and typing sounds) reparented to UW_WespeBarrageTakeover, with the clip grid from clip_atlas.json.

Needs a DLL that has UW_WespeBarrageTakeover, so run it headless in the DebugGame editor:
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
-DisablePlugins=Fab,BpGeneratorUltimate
Re-running replaces both assets.
"""
import json
import os

import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover'
TARGET_FOLDER = FOLDER + '/WespeBarrage'
SOURCE = FOLDER + '/SpacedArmour/WBP_SpacedArmourTakeover'
TARGET_NAME = 'WBP_WespeBarrageTakeover'
TARGET = TARGET_FOLDER + '/' + TARGET_NAME
PARENT_CLASS_PATH = '/Script/RTS_Survival.W_WespeBarrageTakeover'
TEXTURE_NAME = 'T_WespeBarrage_ClipAtlas'
SAVED = os.path.join(unreal.Paths.project_saved_dir(), 'InfoTakeover', 'WespeBarrage')
TEXTURE_SOURCE = os.path.join(SAVED, TEXTURE_NAME + '.png')
ATLAS_INFO = os.path.join(SAVED, 'clip_atlas.json')
CLIP_FPS = 12.0


def log(message):
    unreal.log('WESPE_BARRAGE_ASSET ' + message)


def import_texture():
    if not os.path.isfile(TEXTURE_SOURCE):
        raise RuntimeError('WESPE_BARRAGE_ASSET missing atlas ' + TEXTURE_SOURCE)
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
        raise RuntimeError('WESPE_BARRAGE_ASSET texture import failed')
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('srgb', True)
    # BC1: the frames are opaque; a 3840 x 2160 atlas is about 4 MB on the GPU this way.
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    saved = unreal.EditorAssetLibrary.save_asset(TARGET_FOLDER + '/' + TEXTURE_NAME, False)
    log('texture %dx%d saved: %s' % (texture.blueprint_get_size_x(), texture.blueprint_get_size_y(), saved))


def style_text(widget_bp_path):
    generated = unreal.load_object(None, widget_bp_path + '.' + widget_bp_path.rsplit('/', 1)[1] + '_C')
    style = unreal.get_default_object(generated).get_editor_property('m_style')
    return style.export_text()


def create_widget():
    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if parent_class is None:
        raise RuntimeError('WESPE_BARRAGE_ASSET W_WespeBarrageTakeover is not in this DLL; build DebugGame first')
    with open(ATLAS_INFO) as handle:
        atlas = json.load(handle)
    source_style = style_text(SOURCE)
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        unreal.EditorAssetLibrary.delete_asset(TARGET)
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
    if duplicate is None:
        raise RuntimeError('WESPE_BARRAGE_ASSET duplicate failed')
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicate, parent_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
    generated = unreal.load_object(None, TARGET + '.' + TARGET_NAME + '_C')
    cdo = unreal.get_default_object(generated)
    if style_text(TARGET) != source_style:
        # Copy the skin over explicitly; the reparent reset it.
        style = cdo.get_editor_property('m_style')
        style.import_text(source_style)
        cdo.set_editor_property('m_style', style)
        log('style copied explicitly')
    cdo.set_editor_property('m_clip_atlas', unreal.load_asset(TARGET_FOLDER + '/' + TEXTURE_NAME))
    cdo.set_editor_property('m_clip_columns', atlas['columns'])
    cdo.set_editor_property('m_clip_rows', atlas['rows'])
    cdo.set_editor_property('m_clip_frame_count', atlas['frames'])
    cdo.set_editor_property('m_clip_frames_per_second', CLIP_FPS)
    unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
    generated = unreal.load_object(None, TARGET + '.' + TARGET_NAME + '_C')
    cdo = unreal.get_default_object(generated)
    log('style kept: %s; is a W_WespeBarrageTakeover: %s; clip %s x %s, %s frames' % (
        style_text(TARGET) == source_style, isinstance(cdo, unreal.W_WespeBarrageTakeover),
        cdo.get_editor_property('m_clip_columns'), cdo.get_editor_property('m_clip_rows'),
        cdo.get_editor_property('m_clip_frame_count')))
    saved = unreal.EditorAssetLibrary.save_asset(TARGET, False)
    log('widget saved: %s' % saved)


import_texture()
create_widget()
