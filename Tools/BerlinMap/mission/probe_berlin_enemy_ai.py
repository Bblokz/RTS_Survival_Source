"""Read-only (commandlet): Ger_Berlin's enemy controller setup - decision tree, mission settings, level training (tech
level options and the building types that unlock them), and the BXP types of the Reichstag base buildings."""
import unreal


def log(text):
    unreal.log_warning('BERLIN_AI ' + str(text))


world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin')
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
ecs = [a for a in actors if 'EnemyController' in a.get_class().get_name()]
log(f'enemy controllers: {[(a.get_actor_label(), a.get_class().get_name()) for a in ecs]}')
for ec in ecs:
    tree = ec.get_editor_property('m_stochastic_decision_tree')
    log(f'tree: {tree.get_path_name() if tree else None}')
    s = ec.get_editor_property('m_enemy_ai_mission_settings')
    log('mission settings: ' + s.export_text()[:900])
    lt = ec.get_editor_property('m_enemy_level_training')
    log(f"points start {lt.get_editor_property('training_points_start')} per min "
        f"{lt.get_editor_property('training_points_per_minute')}")
    for field in ('basic_infantry_options', 'light_tank_options', 'medium_tank_options', 'tier2_options',
                  'advanced_infantry_options', 'tier3_options', 'experimental_options'):
        o = lt.get_editor_property(field)
        log(f"{field}: unlock {[str(t) for t in o.get_editor_property('types_unlocking_this_level')]} "
            f"tanks {[str(t) for t in o.get_editor_property('trainable_tank_subtypes')]} "
            f"squads {[str(t) for t in o.get_editor_property('trainable_squad_subtypes')]}")
    for prop in ('owning_player', 'player'):
        try:
            log(f'{prop}: {ec.get_editor_property(prop)}')
        except Exception:
            pass
for a in actors:
    if a.get_actor_label().startswith('RT_Base_'):
        t = None
        for getter in ('get_building_expansion_type', 'get_bxp_type'):
            if hasattr(a, getter):
                try:
                    t = getattr(a, getter)()
                except Exception as e:
                    t = f'err {e}'
        try:
            t2 = a.get_editor_property('building_expansion_type')
        except Exception:
            t2 = None
        trainer = [c for c in a.get_components_by_class(unreal.ActorComponent)
                   if 'Trainer' in c.get_class().get_name()]
        log(f'{a.get_actor_label()}: type {t} / {t2} trainer {[c.get_class().get_name() for c in trainer]}')
