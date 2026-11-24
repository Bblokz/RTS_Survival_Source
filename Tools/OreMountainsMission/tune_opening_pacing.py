"""Owner tuning 2026-11-24 (after the first timeline report), editor Python with Ger_2ndOreMountains open:
- convoy 1 starts later: ORE_Convoy1 Timing.FirstConvoyDelay 80 s (was 20);
- the light counterattack is faster: shorter preparation and wave gap on every difficulty, and the escape-bonus T-70
  arrives with the first wave (normal 25 s);
- the Opening's new drop defaults (start vehicles flown in early): DropIntervalSeconds 3, FollowUpDrops 10,
  ExpectedDrops 11, on the class defaults and the map instance.
Re-runnable. Saves the counterattack settings asset, the two stage Blueprints and the level.
"""
import unreal

MAP_NAME = 'Ger_2ndOreMountains'
BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/'
CONVOY1_DELAY = 80.0
# difficulty field: (seconds until the Soviets arrive, seconds between the 2 waves); before: 60/70, 45/60, 35/60, 25/50
LIGHT_COUNTERATTACK = {'new_to_rts': (35, 40), 'normal': (25, 35), 'hard': (20, 30), 'brutal': (15, 30),
                       'ironman': (15, 30)}
OPENING_DEFAULTS = {'DropIntervalSeconds': 3, 'FollowUpDrops': 10, 'ExpectedDrops': 11}


def log(text):
    print('ORE_TUNE ' + str(text))


def set_convoy1_delay(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.EnemyConvoyRoute):
        if str(actor.get_editor_property('route_id')) != 'ORE_Convoy1':
            continue
        actor.modify()
        timing = actor.get_editor_property('timing')
        timing.set_editor_property('first_convoy_delay', CONVOY1_DELAY)
        actor.set_editor_property('timing', timing)
        log('ORE_Convoy1 first convoy delay = %s' % actor.get_editor_property('timing').get_editor_property(
            'first_convoy_delay'))


def tune_light_counterattack():
    path = '/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains/DA_Ore_LightCounterattack'
    settings = unreal.load_asset(path)
    for field, (arrive, between) in LIGHT_COUNTERATTACK.items():
        difficulty = settings.get_editor_property(field)
        difficulty.set_editor_property('seconds_until_soviets_arrive', arrive)
        difficulty.set_editor_property('seconds_between_waves', between)
        settings.set_editor_property(field, difficulty)
    log('light counterattack saved %s' % unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False))
    return LIGHT_COUNTERATTACK['normal'][0]


def set_stage_defaults(stage, values):
    path = BASE + stage
    blueprint = unreal.load_asset(path)
    cdo = unreal.get_default_object(blueprint.generated_class())
    for name, value in values.items():
        cdo.set_editor_property(name, value)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    log('%s defaults %s saved %s' % (stage, values, unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)))
    return blueprint.generated_class()


def sync_instances(world, values_by_class):
    managers = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                if a.get_class().get_name() == 'BP_MissionManager_C']
    manager = managers[0]
    manager.modify()
    for mission in manager.get_editor_property('missions'):
        if mission is None:
            continue
        values = values_by_class.get(mission.get_class())
        if not values:
            continue
        for name, value in values.items():
            mission.set_editor_property(name, value)
        log('instance %s <- %s' % (mission.get_class().get_name(), values))


def run():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_name() != MAP_NAME:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(
            '/Game/RTS_Survival/Maps/GerMissions/' + MAP_NAME)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    set_convoy1_delay(world)
    arrive = tune_light_counterattack()
    bonus = {'EscapeBonusDelaySeconds': float(arrive)}
    counter_class = set_stage_defaults('Mission_Ore_LightCounterattack', bonus)
    opening_class = set_stage_defaults('Mission_Ore_Opening', OPENING_DEFAULTS)
    sync_instances(world, {counter_class: bonus, opening_class: OPENING_DEFAULTS})
    log('level saved %s' % unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())


run()
