"""Creates the spaced armour takeover assets in GameUI/InfoTakeover/SpacedArmour:
- T_SpacedArmour_PlateShotOff: the render of a Panzer II Luchs losing a side plate, imported from
  Saved/InfoTakeover/SpacedArmour/Previews/T_SpacedArmour_PlateShotOff.png with the UI settings of T_CoverPreview_Green;
- WBP_SpacedArmourTakeover: a copy of WBP_VehicleInfoTakeover (so it carries the same M_Style skin, Proto text styles
  and typing sounds) reparented to UW_SpacedArmourTakeover.

Needs a DLL that has UW_SpacedArmourTakeover, so run it headless in the DebugGame editor, e.g.
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
-DisablePlugins=Fab,BpGeneratorUltimate
Re-running replaces both assets.
"""
import os

import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover'
TARGET_FOLDER = FOLDER + '/SpacedArmour'
SOURCE = FOLDER + '/WBP_VehicleInfoTakeover'
TARGET_NAME = 'WBP_SpacedArmourTakeover'
TARGET = TARGET_FOLDER + '/' + TARGET_NAME
PARENT_CLASS_PATH = '/Script/RTS_Survival.W_SpacedArmourTakeover'
TEXTURE_NAME = 'T_SpacedArmour_PlateShotOff'
TEXTURE_SOURCE = os.path.join(unreal.Paths.project_saved_dir(), 'InfoTakeover', 'SpacedArmour', 'Previews',
                              TEXTURE_NAME + '.png')
REFERENCE_TEXTURE = FOLDER + '/Cover/T_CoverPreview_Green'
TEXTURE_SETTINGS = ['compression_settings', 'lod_group', 'mip_gen_settings', 'srgb', 'never_stream', 'filter',
                    'address_x', 'address_y']


def import_texture():
    if not os.path.isfile(TEXTURE_SOURCE):
        raise RuntimeError('SPACED_ARMOUR_ASSET missing render ' + TEXTURE_SOURCE)
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
        raise RuntimeError('SPACED_ARMOUR_ASSET texture import failed')
    reference = unreal.load_asset(REFERENCE_TEXTURE)
    for setting in TEXTURE_SETTINGS:
        try:
            value = reference.get_editor_property(setting)
            texture.set_editor_property(setting, value)
            unreal.log('SPACED_ARMOUR_ASSET texture %s = %s' % (setting, value))
        except Exception as error:  # A setting missing on this engine version is not fatal.
            unreal.log_warning('SPACED_ARMOUR_ASSET texture setting %s not copied: %s' % (setting, error))
    # set_editor_property already ran PostEditChange, which rebuilds the platform data.
    saved = unreal.EditorAssetLibrary.save_asset(TARGET_FOLDER + '/' + TEXTURE_NAME, False)
    unreal.log('SPACED_ARMOUR_ASSET texture %dx%d saved: %s' % (texture.blueprint_get_size_x(),
                                                               texture.blueprint_get_size_y(), saved))


def style_text(widget_bp_path):
    generated = unreal.load_object(None, widget_bp_path + '.' + widget_bp_path.rsplit('/', 1)[1] + '_C')
    style = unreal.get_default_object(generated).get_editor_property('m_style')
    return style.export_text()


def create_widget():
    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if parent_class is None:
        raise RuntimeError('SPACED_ARMOUR_ASSET W_SpacedArmourTakeover is not in this DLL; build DebugGame first')
    source_style = style_text(SOURCE)
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        unreal.EditorAssetLibrary.delete_asset(TARGET)
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
    if duplicate is None:
        raise RuntimeError('SPACED_ARMOUR_ASSET duplicate failed')
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicate, parent_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
    target_style = style_text(TARGET)
    same_style = target_style == source_style
    unreal.log('SPACED_ARMOUR_ASSET style kept after reparent: %s (%d chars)' % (same_style, len(target_style)))
    if not same_style:
        # Copy the skin over explicitly; the reparent reset it.
        generated = unreal.load_object(None, TARGET + '.' + TARGET_NAME + '_C')
        cdo = unreal.get_default_object(generated)
        style = cdo.get_editor_property('m_style')
        style.import_text(source_style)
        cdo.set_editor_property('m_style', style)
        unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
        unreal.log('SPACED_ARMOUR_ASSET style copied explicitly: %s' % (style_text(TARGET) == source_style))
    generated = unreal.load_object(None, TARGET + '.' + TARGET_NAME + '_C')
    is_takeover = isinstance(unreal.get_default_object(generated), unreal.W_SpacedArmourTakeover)
    unreal.log('SPACED_ARMOUR_ASSET is a W_SpacedArmourTakeover: %s' % is_takeover)
    saved = unreal.EditorAssetLibrary.save_asset(TARGET, False)
    unreal.log('SPACED_ARMOUR_ASSET widget saved: %s' % saved)


import_texture()
create_widget()
