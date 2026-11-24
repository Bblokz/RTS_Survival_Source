"""Creates WBP_CoverTakeover (in GameUI/InfoTakeover/Cover): a copy of WBP_VehicleInfoTakeover (so it carries the same M_Style skin, Proto
text styles and typing sounds) reparented to UW_CoverTakeover.

Needs a DLL that has UW_CoverTakeover, so run it headless in the DebugGame editor, e.g.
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
-DisablePlugins=Fab,BpGeneratorUltimate
Re-running replaces the asset.
"""
import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover'
SOURCE = FOLDER + '/WBP_VehicleInfoTakeover'
TARGET_NAME = 'WBP_CoverTakeover'
TARGET = FOLDER + '/Cover/' + TARGET_NAME
PARENT_CLASS_PATH = '/Script/RTS_Survival.W_CoverTakeover'


def style_text(widget_bp_path):
    generated = unreal.load_object(None, widget_bp_path + '.' + widget_bp_path.rsplit('/', 1)[1] + '_C')
    style = unreal.get_default_object(generated).get_editor_property('m_style')
    return style.export_text()


def main():
    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if parent_class is None:
        raise RuntimeError('COVER_TAKEOVER_ASSET W_CoverTakeover is not in this DLL; build DebugGame first')
    source_style = style_text(SOURCE)
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        unreal.EditorAssetLibrary.delete_asset(TARGET)
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
    if duplicate is None:
        raise RuntimeError('COVER_TAKEOVER_ASSET duplicate failed')
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicate, parent_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
    target_style = style_text(TARGET)
    same_style = target_style == source_style
    unreal.log('COVER_TAKEOVER_ASSET style kept after reparent: %s (%d chars)' % (same_style, len(target_style)))
    if not same_style:
        # Copy the skin over explicitly; the reparent reset it.
        generated = unreal.load_object(None, TARGET + '.' + TARGET_NAME + '_C')
        cdo = unreal.get_default_object(generated)
        style = cdo.get_editor_property('m_style')
        style.import_text(source_style)
        cdo.set_editor_property('m_style', style)
        unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
        unreal.log('COVER_TAKEOVER_ASSET style copied explicitly: %s' % (style_text(TARGET) == source_style))
    generated = unreal.load_object(None, TARGET + '.' + TARGET_NAME + '_C')
    is_cover_takeover = isinstance(unreal.get_default_object(generated), unreal.W_CoverTakeover)
    unreal.log('COVER_TAKEOVER_ASSET is a W_CoverTakeover: %s' % is_cover_takeover)
    saved = unreal.EditorAssetLibrary.save_asset(TARGET, False)
    unreal.log('COVER_TAKEOVER_ASSET saved: %s' % saved)


main()
