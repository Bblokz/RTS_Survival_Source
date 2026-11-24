"""Lead check (2026-11-12) that the hole guard (UWallHoleDamageComponent::GetDoesHoleTouchWallMesh) keeps holes on
walls, towers and trees: debug hits on traced surface points must still cut; a point in the air beside the mesh
(inside its bounds where possible) must not.

Throwaway DebugGame editor only:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
"""
import json
import math
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/guard_walls_result.json'
BP = '/Game/RTS_Survival/Blueprints/'
SUBJECTS = [
    BP + 'Buildings/Walls/RusWalls/Rus_Wall_Concrete_Long_Holes.Rus_Wall_Concrete_Long_Holes_C',
    BP + 'Buildings/BuildingExpansions/RusBunkers/Rus_BXP_GuardTower.Rus_BXP_GuardTower_C',
    BP + 'Environment/DestructableEnvActor/BP_Scorch_Dest_DeadTree.BP_Scorch_Dest_DeadTree_C',
    BP + 'Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit.Rus_BXP_53KPit_C',
    BP + 'Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Pak40.Ger_BXP_GerPit_Pak40_C',
]
SPOT0 = unreal.Vector(11500, -7000, 0)
SPACING = 1600.0
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': []}


def log(message):
    S['log'].append(message)
    unreal.log('GUARDWALLS ' + message)


def finish():
    try:
        json.dump(S['log'], open(OUT, 'w'), indent=1)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def ground_z(world, spot):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(spot.x, spot.y, 20000),
                                                 unreal.Vector(spot.x, spot.y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 100.0


def wall_mesh(actor, holes):
    name = str(holes.get_editor_property('WallMeshComponentName'))
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if component.get_name() == name:
            return component
    return None


def check(actor):
    holes = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    mesh = wall_mesh(actor, holes) if holes else None
    if mesh is None:
        log(f'{actor.get_name()}: no hole component or mesh')
        return
    origin, extent = mesh.get_local_bounds()
    centre = mesh.get_world_transform().transform_location((origin + extent) * 0.5)
    reach = max(extent.x - origin.x, extent.y - origin.y) * 0.5 + 600
    mid_z = (origin.z + extent.z) * 0.5
    cut = 0
    shots = 0
    for angle in (0, 90, 180, 270):
        out = unreal.Vector(math.cos(math.radians(angle)), math.sin(math.radians(angle)), 0)
        start = unreal.Vector(centre.x, centre.y, mesh.get_world_transform().transform_location(
            unreal.Vector(0, 0, mid_z)).z) + out * reach
        result = mesh.line_trace_component(start, start - out * reach * 2, True, False, False)
        if not result:
            log(f'{actor.get_name()} angle {angle}: trace missed the mesh')
            continue
        shots += 1
        removed = holes.debug_fire_test_hit(result[0], out * -1.0, 75.0)
        cut += 1 if removed > 0 else 0
        log(f'{actor.get_name()} angle {angle}: surface hit removed {removed:.2f} m2')
    # Air point: 3 m above the bounds top, so the 64 cm sphere is clear of everything.
    air = mesh.get_world_transform().transform_location(unreal.Vector((origin.x + extent.x) * 0.5,
                                                                      (origin.y + extent.y) * 0.5, extent.z + 300))
    air_removed = holes.debug_fire_test_hit(air, unreal.Vector(0, 0, -1), 75.0)
    log(f'{actor.get_name()} RESULT surface cut {cut}/{shots}, air removed {air_removed:.2f} '
        f'(query={mesh.is_query_collision_enabled()}) holes={holes.get_hole_count()}')


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done':
        return
    try:
        if S['phase'] == 'wait_load' and S['t'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            for index, path in enumerate(SUBJECTS):
                spot = SPOT0 + unreal.Vector(index * SPACING, 0, 0)
                spot.z = ground_z(world, spot)
                placed = actors.spawn_actor_from_class(unreal.load_class(None, path), spot)
                rts = placed.get_component_by_class(unreal.RTSComponent)
                if rts:
                    rts.set_editor_property('owning_player', 1)
                log(f'placed {placed.get_name()}')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif S['phase'] == 'wait_pie':
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                for path in SUBJECTS:
                    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, path)):
                        if actor.get_class().get_path_name() == path:
                            check(actor)
                finish()
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
    except Exception as error:
        log(f'error {error}')
        finish()


S['handle'] = unreal.register_slate_post_tick_callback(tick)
log('script loaded')
