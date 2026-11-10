"""Throwaway-editor test: holes/chunks close on repair (UHealthComponent heal event) and materials go back."""
# Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one:
#   UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle
#     -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
import json
import math
import unreal

OUT = r'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits53K/pit_repair_result.json'
BASE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
SUBJECTS = [
    ('hole', BASE + 'Rus53KPit/Rus_BXP_53KPit.Rus_BXP_53KPit_C', unreal.Vector(12500, -7000, 73)),
    ('hole', BASE + 'Rus53KPit/Rus_BXP_53KSectorPit.Rus_BXP_53KSectorPit_C', unreal.Vector(14500, -7000, 142)),
    ('chunk', BASE + 'RusBunkers/Rus_BXP_MGBunker.Rus_BXP_MGBunker_C', unreal.Vector(16500, -7000, 142)),
]
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'labels': []}


def log(message):
    S['log'].append(message)
    unreal.log('PITREPAIR ' + message)


def finish():
    with open(OUT, 'w', encoding='utf-8') as handle:
        json.dump(S['log'], handle, indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def subjects_in(world):
    names =[path.split('.')[-1] for _, path, _ in SUBJECTS]
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() in names]


def damage_comp(actor):
    hole = actor.get_component_by_class(unreal.WallHoleDamageComponent)
    return ('hole', hole) if hole else ('chunk', actor.get_component_by_class(unreal.WallChunkDamageComponent))


def mesh_comp(actor):
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        if comp.get_name() == 'BuildingMeshComponent':
            return comp
    return actor.get_component_by_class(unreal.StaticMeshComponent)


def report(world, tag):
    for actor in subjects_in(world):
        kind, comp = damage_comp(actor)
        health = actor.get_component_by_class(unreal.HealthComponent)
        mesh = mesh_comp(actor)
        count = comp.get_hole_count() if kind == 'hole' else comp.get_hidden_chunk_count()
        mats = [mesh.get_material(i).get_name() if mesh.get_material(i) else None for i in range(mesh.get_num_materials())]
        log(f'{tag} {actor.get_class().get_name()} hp={health.get_current_health():.0f}/{health.get_max_health():.0f} '
            f'{kind}s={count} mats={mats}')


def cut_damage(world):
    for actor in subjects_in(world):
        kind, comp = damage_comp(actor)
        health = actor.get_component_by_class(unreal.HealthComponent)
        health.set_current_health(health.get_max_health() * 0.3)
        origin = actor.get_actor_location()
        hits = 0
        for angle in (150, 180, 210, 240, 270, 300, 330, 0):
            radians = math.radians(angle)
            direction = unreal.Vector(math.cos(radians), math.sin(radians), 0)
            for height in (40, 120, 220):
                start = origin + direction * 1500 + unreal.Vector(0, 0, height)
                end = origin + unreal.Vector(0, 0, height)
                hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                             False, [], unreal.DrawDebugTrace.NONE, True)
                if hit is None:
                    continue
                values = hit.to_tuple()
                hit_actor = values[9] if isinstance(values[9], unreal.Actor) else None
                hit_comp = values[10] if len(values) > 10 else None
                owner = hit_actor or (hit_comp.get_owner() if hit_comp else None)
                if owner != actor:
                    continue
                comp.debug_fire_test_hit(values[4], direction * -1.0, 75.0)
                hits += 1
                break
        log(f'cut {actor.get_class().get_name()} debug_hits={hits}')


def heal_step(world, target_fraction):
    """One real heal (USingleHealBehaviour, 10 HP through UHealthComponent::Heal) per subject below the target.
    @return Whether every subject reached the target."""
    all_done = True
    for actor in subjects_in(world):
        health = actor.get_component_by_class(unreal.HealthComponent)
        if health.get_current_health() >= health.get_max_health() * target_fraction - 0.01:
            continue
        all_done = False
        behaviours = actor.get_component_by_class(unreal.BehaviourComp)
        behaviours.remove_behaviour(unreal.SingleHealBehaviour)
        behaviours.add_behaviour(unreal.SingleHealBehaviour)
    return all_done


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            for _, path, location in SUBJECTS:
                placed = actors.spawn_actor_from_class(unreal.load_class(None, path), location)
                log(f'placed {placed.get_name() if placed else None}')
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                log(f'pie world {world.get_name()} subjects={len(subjects_in(world))}')
                report(world, 'start')
                cut_damage(world)
                report(world, 'damaged30')
                S['phase'], S['mark'] = 'heal1', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'heal1':
            world = game_world()
            if heal_step(world, 0.7) or S['t'] - S['mark'] > 90.0:
                report(world, 'healed70')
                S['phase'], S['mark'] = 'heal2', S['t']
        elif phase == 'heal2':
            world = game_world()
            if heal_step(world, 1.0) or S['t'] - S['mark'] > 90.0:
                report(world, 'healed100')
                finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
