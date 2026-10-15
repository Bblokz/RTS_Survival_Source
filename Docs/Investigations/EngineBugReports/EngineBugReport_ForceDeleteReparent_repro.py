"""Repro for: ForceDelete re-parents unrelated Blueprints after a failed force-delete (UE 5.5).

Run in the FULL editor (not a commandlet - see the second issue in the report), e.g.:
    UnrealEditor.exe <Project>.uproject -unattended -ExecutePythonScript="<path>/EngineBugReport_ForceDeleteReparent_repro.py"
Set SOURCE_BLUEPRINT to any Blueprint in the project. Nothing is saved; the editor quits at the end.
The Movie Render Pipeline plugin was enabled in our project; its widget Blueprint was the victim."""
import time
import unreal

SOURCE_BLUEPRINT = '/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/W_BP_TrainingBuildingEntry'
SOURCE_WIDGET_IN_TREE = 'M_EntryButton'   # any object inside the source Blueprint, used to keep the copy referenced
TEMP_FOLDER = '/Game/__ReproForceDelete_%d' % int(time.time())
COPY = TEMP_FOLDER + '/BP_ReproCopy'


def log(message):
    unreal.log('REPRO ' + str(message))


def native_parent(blueprint):
    try:
        generated = blueprint.generated_class()
        return type(unreal.get_default_object(generated)).__name__ if generated else 'NO_GENERATED_CLASS'
    except Exception as error:
        return 'ERR:%s' % error


def parents_of_loaded_blueprints():
    return {bp.get_path_name(): native_parent(bp) for bp in unreal.ObjectIterator(unreal.Blueprint)}


unreal.EditorAssetLibrary.duplicate_asset(SOURCE_BLUEPRINT, COPY)
# 1. keep the copy referenced so the first force-delete fails with "is in use"
held_reference = unreal.find_object(None, COPY + '.BP_ReproCopy:WidgetTree.' + SOURCE_WIDGET_IN_TREE)
log('holding %s' % held_reference)
before = parents_of_loaded_blueprints()

# 2. first force-delete: fails ("potentially corrupt") but has already nulled the Blueprint's GeneratedClass
log('first delete -> %s' % unreal.EditorAssetLibrary.delete_directory(TEMP_FOLDER))
zombie = unreal.find_object(None, COPY + '.BP_ReproCopy')
log('zombie alive=%s generated class=%s' % (zombie is not None, zombie.generated_class() if zombie else None))

# 3. second force-delete of the zombie: re-parents every Blueprint whose class has a null super
log('second delete -> %s' % unreal.EditorAssetLibrary.delete_directory(TEMP_FOLDER))
after = parents_of_loaded_blueprints()
for path in sorted(after):
    if not path.startswith(TEMP_FOLDER) and before.get(path) != after[path]:
        log('RE-PARENTED %s : %s -> %s' % (path, before.get(path), after[path]))
unreal.SystemLibrary.quit_editor()
