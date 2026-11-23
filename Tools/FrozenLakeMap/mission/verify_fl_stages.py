"""Read-only check of Ger_FrozenLake after wiring (run headless with -ExecutePythonScript; saves nothing).
Logs the mission list and every placed actor with an FL_ tag: class, parent chain, health."""
import sys

import unreal

sys.path.append('D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap/mission')
import wire_fl_stages as wfs  # noqa: E402


def log(text):
    unreal.log_warning('FL_VERIFY ' + text)


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
log('map ' + world.get_name())
for line in wfs.describe(world):
    log(line)
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    tags = [str(t) for t in actor.tags if str(t).startswith('FL_')]
    if not tags:
        continue
    destructible = isinstance(actor, unreal.DestructableEnvActor)
    health = actor.get_editor_property('health') if destructible else None
    log(f'{actor.get_actor_label()} tags={tags} class={actor.get_class().get_name()} destructible={destructible} '
        f'health={health}')
