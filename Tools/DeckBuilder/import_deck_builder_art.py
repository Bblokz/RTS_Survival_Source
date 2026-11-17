"""Imports the deck builder card menu's art into /Game/RTS_Survival/Textures/GameUI/DeckBuilder.

Source PNGs: UISkins/13_Frontstahl_Gunmetal/DeckBuilder/T_Deck_*.png
  T_Deck_Panel               the plate behind each column (the Frontstahl card holder plate)
  T_Deck_CardFrame_<rarity>  the unit / technology card frame (Frontstahl T_Panel02), plus Vacant
  T_Deck_Spine_<rarity>      the narrow variant card with its painted spine (made with UISkins/_generator/cards.py,
                             see Saved/CardDesigns/make_spine_frames.py for the shape and the right-hand stripe)
  T_Deck_Section_<section>   the building pictures of the rail
  T_Deck_Part_*              half-size copies of the hangar variant screen's slice textures (rows, rail entries,
                             icon well, buttons, tabs). Slate draws box brush corners at texture size, so these are
                             one texel per UI unit; T_Deck_Panel is scaled for the same reason (corner 26 units).

FDeckBuilderStyle (Source/RTS_Survival/CardSystem/DeckBuilder/DeckBuilderStyle.cpp) points at these by name.
Re-runnable: existing textures are replaced. Run as a commandlet with run_import_deck_builder_art.ps1.
"""
import glob
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE_FOLDER = os.path.abspath(os.path.join(HERE, '..', '..', '..', 'UISkins', '13_Frontstahl_Gunmetal', 'DeckBuilder'))
DESTINATION = '/Game/RTS_Survival/Textures/GameUI/DeckBuilder'
EXPECTED_COUNT = 33


def import_textures():
    sources = sorted(glob.glob(os.path.join(SOURCE_FOLDER, 'T_Deck_*.png')))
    if len(sources) != EXPECTED_COUNT:
        unreal.log_error('[DeckArt] expected %d PNGs in %s, found %d' % (EXPECTED_COUNT, SOURCE_FOLDER, len(sources)))
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
            unreal.log_error('[DeckArt] import failed: %s' % path)
            continue
        # Same settings as the skin's other UI textures: UI group, never streamed, uncompressed sRGB, simple mips
        # (the cards are also drawn smaller than their art in the collection).
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('never_stream', True)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('srgb', True)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        imported += 1
        unreal.log('[DeckArt] imported %s' % path)
    unreal.log('[DeckArt] done: %d of %d textures' % (imported, len(tasks)))


import_textures()
