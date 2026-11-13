"""Agent B, throwaway editor: list RTS actors (owner, class, location) in UT_VehicleObstacle and quit."""
import json
import unreal
OUT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/agentB_map_actors.json'
S = {'t': 0.0, 'done': False}
def tick(delta):
    S['t'] += delta
    if S['done'] or S['t'] < 15.0:
        return
    S['done'] = True
    rows = []
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        rows.append(['map', world.get_path_name()])
        for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
            rts = actor.get_component_by_class(unreal.RTSComponent)
            loc = actor.get_actor_location()
            name = actor.get_class().get_name()
            if rts is not None or 'Spawn' in name or 'Wave' in name or 'Wall' in name:
                rows.append([name, actor.get_actor_label(), rts.get_editor_property('owning_player') if rts else None,
                             round(loc.x), round(loc.y), round(loc.z)])
    except Exception as error:  # noqa: BLE001
        rows.append(['ERROR', repr(error)])
    json.dump(rows, open(OUT, 'w'), indent=0)
    unreal.SystemLibrary.quit_editor()
unreal.register_slate_post_tick_callback(tick)
