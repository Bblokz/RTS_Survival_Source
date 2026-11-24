"""Creates GameUI/InfoTakeover/BxpPackUp/WBP_BxpPackUpTakeover: a copy of WBP_VehicleInfoTakeover (so it
carries the same M_Style skin, Proto text styles and typing sounds) reparented to UW_BxpPackUpTakeover.

Needs an editor DLL that has UW_BxpPackUpTakeover. Either:
- in the live editor after the rebuild, through UECP execute_python: exec(open(r'<this file>').read())
  (the script imports unreal itself), or
- headless, when the DebugGame DLL has the class:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
  -DisablePlugins=Fab,BpGeneratorUltimate
Re-running replaces the asset. The illustration (WBP_BxpPack_Loop) is built separately by
bxp_packup_illustration.py and bxp_packup_transform_tracks.py.
"""
import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover'
TARGET_FOLDER = FOLDER + '/BxpPackUp'
SOURCE = FOLDER + '/WBP_VehicleInfoTakeover'
TARGET_NAME = 'WBP_BxpPackUpTakeover'
TARGET = TARGET_FOLDER + '/' + TARGET_NAME
PARENT_CLASS_PATH = '/Script/RTS_Survival.W_BxpPackUpTakeover'


def style_text(widget_bp_path):
    generated = unreal.load_object(None, widget_bp_path + '.' + widget_bp_path.rsplit('/', 1)[1] + '_C')
    style = unreal.get_default_object(generated).get_editor_property('m_style')
    return style.export_text()


def create_bxp_packup_widget():
    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if parent_class is None:
        raise RuntimeError('BXP_PACKUP_TAKEOVER_ASSET W_BxpPackUpTakeover is not in this DLL; rebuild first')
    source_style = style_text(SOURCE)
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        unreal.EditorAssetLibrary.delete_asset(TARGET)
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
    if duplicate is None:
        raise RuntimeError('BXP_PACKUP_TAKEOVER_ASSET duplicate failed')
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicate, parent_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
    target_style = style_text(TARGET)
    same_style = target_style == source_style
    unreal.log('BXP_PACKUP_TAKEOVER_ASSET style kept after reparent: %s (%d chars)' % (same_style, len(target_style)))
    if not same_style:
        # Copy the skin over explicitly; the reparent reset it.
        generated = unreal.load_object(None, TARGET + '.' + TARGET_NAME + '_C')
        cdo = unreal.get_default_object(generated)
        style = cdo.get_editor_property('m_style')
        style.import_text(source_style)
        cdo.set_editor_property('m_style', style)
        unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
        unreal.log('BXP_PACKUP_TAKEOVER_ASSET style copied explicitly: %s' % (style_text(TARGET) == source_style))
    generated = unreal.load_object(None, TARGET + '.' + TARGET_NAME + '_C')
    is_takeover = isinstance(unreal.get_default_object(generated), unreal.W_BxpPackUpTakeover)
    unreal.log('BXP_PACKUP_TAKEOVER_ASSET is a W_BxpPackUpTakeover: %s' % is_takeover)
    saved = unreal.EditorAssetLibrary.save_asset(TARGET, False)
    unreal.log('BXP_PACKUP_TAKEOVER_ASSET saved: %s' % saved)
    print('BXP_PACKUP_TAKEOVER_ASSET is_takeover=%s same_style=%s saved=%s' % (is_takeover, same_style, saved))


create_bxp_packup_widget()
