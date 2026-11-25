"""Creates the two Ore Mountains takeover Blueprints:
- GameUI/InfoTakeover/CapturingTrucks/WBP_CapturingTrucksTakeover (UW_CapturingTrucksTakeover, the capturing trucks
  field manual; its diagram is plain UMG built in C++, so there is no texture to import);
- GameUI/InfoTakeover/Document/WBP_DocumentTakeover (UW_DocumentTakeover, the generic captured-document takeover the
  Ore manifest uses).
Both are copies of WBP_SpacedArmourTakeover (already on the "Rotsignal" M_Style skin, Proto text styles and typing
sounds) reparented to the new C++ class.

Needs a DLL that has both classes, so run it headless in the DebugGame editor:
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
-DisablePlugins=Fab,BpGeneratorUltimate
Re-running replaces both assets.
"""
import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover'
SOURCE = FOLDER + '/SpacedArmour/WBP_SpacedArmourTakeover'
TARGETS = [
    (FOLDER + '/CapturingTrucks', 'WBP_CapturingTrucksTakeover', '/Script/RTS_Survival.W_CapturingTrucksTakeover'),
    (FOLDER + '/Document', 'WBP_DocumentTakeover', '/Script/RTS_Survival.W_DocumentTakeover'),
]


def log(message):
    unreal.log('ORE_TAKEOVER_ASSET ' + message)


def style_text(widget_bp_path):
    generated = unreal.load_object(None, widget_bp_path + '.' + widget_bp_path.rsplit('/', 1)[1] + '_C')
    style = unreal.get_default_object(generated).get_editor_property('m_style')
    return style.export_text()


def create_widget(target_folder, target_name, parent_class_path, source_style):
    target = target_folder + '/' + target_name
    parent_class = unreal.load_class(None, parent_class_path)
    if parent_class is None:
        raise RuntimeError('ORE_TAKEOVER_ASSET %s is not in this DLL; build DebugGame first' % parent_class_path)
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        unreal.EditorAssetLibrary.delete_asset(target)
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, target)
    if duplicate is None:
        raise RuntimeError('ORE_TAKEOVER_ASSET duplicate failed for ' + target)
    unreal.BlueprintEditorLibrary.reparent_blueprint(duplicate, parent_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
    if style_text(target) != source_style:
        # Copy the skin over explicitly; the reparent reset it.
        generated = unreal.load_object(None, target + '.' + target_name + '_C')
        cdo = unreal.get_default_object(generated)
        style = cdo.get_editor_property('m_style')
        style.import_text(source_style)
        cdo.set_editor_property('m_style', style)
        unreal.BlueprintEditorLibrary.compile_blueprint(duplicate)
        log('%s: style copied explicitly' % target_name)
    generated = unreal.load_object(None, target + '.' + target_name + '_C')
    cdo = unreal.get_default_object(generated)
    log('%s: style kept: %s; parent ok: %s' % (
        target_name, style_text(target) == source_style, isinstance(cdo, getattr(unreal, parent_class_path.rsplit('.', 1)[1]))))
    saved = unreal.EditorAssetLibrary.save_asset(target, False)
    log('%s saved: %s' % (target_name, saved))


SOURCE_STYLE = style_text(SOURCE)
for folder, name, parent in TARGETS:
    create_widget(folder, name, parent, SOURCE_STYLE)
