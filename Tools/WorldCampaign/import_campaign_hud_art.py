"""Imports the campaign HUD art into /Game/RTS_Survival/Textures/GameUI/WorldCampaign.

Source PNGs: UISkins/13_Frontstahl_Gunmetal/WorldCampaign/T_Campaign_*.png (made by make_campaign_hud_art.py).
FWorldCampaignUILook (WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.cpp) points at these by name.
Re-runnable: existing textures are replaced. Run as a commandlet with run_import_campaign_hud_art.ps1.
"""
import glob
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE_FOLDER = os.path.abspath(os.path.join(HERE, '..', '..', '..', 'UISkins', '13_Frontstahl_Gunmetal', 'WorldCampaign'))
DESTINATION = '/Game/RTS_Survival/Textures/GameUI/WorldCampaign'
EXPECTED_COUNT = 3


def import_textures():
    sources = sorted(glob.glob(os.path.join(SOURCE_FOLDER, 'T_Campaign_*.png')))
    if len(sources) != EXPECTED_COUNT:
        unreal.log_error('[CampaignArt] expected %d PNGs in %s, found %d' % (EXPECTED_COUNT, SOURCE_FOLDER, len(sources)))
    tasks = []
    for source in sources:
        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = DESTINATION
        task.destination_name = os.path.splitext(os.path.basename(source))[0]
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    imported = 0
    for task in tasks:
        path = task.destination_path + '/' + task.destination_name
        texture = unreal.load_asset(path)
        if texture is None:
            unreal.log_error('[CampaignArt] import failed: %s' % path)
            continue
        # Same settings as the skin's other UI textures: UI group, never streamed, uncompressed sRGB, simple mips.
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('never_stream', True)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('srgb', True)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        imported += 1
        unreal.log('[CampaignArt] imported %s' % path)
    unreal.log('[CampaignArt] done: %d of %d textures' % (imported, len(tasks)))


import_textures()
