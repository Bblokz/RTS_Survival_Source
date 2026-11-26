"""Ger_IntroMission squad safety net (user request 2026-11-25).

User: "up until the base building part of this map, if at any point the player has only one vulture squad left or no
vulture squads left at all, we then send in a paratrooper plane from which new squads will come, so that in total the
player will have 3 squads."

Sets AMissionManager::PlayerSquadSafetyNet on the map's BP_MissionManager (C++ UPlayerSquadSafetyNetSubsystem):
  Squad_Ger_Vultures, help at <= 1, top up to 3, GA_GerParatroopers, until Mission_Intro_ClearSovietRig completes
  (its completion starts Mission_Intro_BaseBuilding), drop at the landing point nearest the player's Vultures.
Landing points: flat open ground (<= 0.5 m height range within 8 m) behind the front along the player's route:
  start area, before the first move-to, the hangar road, short of the ramp up to the Soviet ridge base.

Exec in a throwaway editor with the map open (rear_editor_server / rear_cmd, after rear_lib.py), then save the map once
the navmesh is built (save_level after nav_ready()).
"""
import unreal

CLEAR_SOVIET_RIG = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/Mission_Intro_ClearSovietRig'
LANDING_POINTS = [(-25000.0, 4000.0, 150.0), (-18000.0, -500.0, 150.0), (-14500.0, -200.0, 150.0),
                  (-11000.0, 0.0, 150.0)]


def ability_enum():
    """EGlobalAbility shares its Python name with the UGlobalAbility class: take the enum type from a field value."""
    return type(unreal.EnemyAirDropDeliverySettings().get_editor_property('cargo_plane_ability'))


def apply_map():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    managers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MissionManager)
    if len(managers) != 1:
        raise RuntimeError('expected one MissionManager, found %d' % len(managers))
    manager = managers[0]
    net = unreal.PlayerSquadSafetyNetSettings()
    net.set_editor_property('enabled', True)
    net.set_editor_property('squad_subtype', unreal.SquadSubtype.SQUAD_GER_VULTURES)
    net.set_editor_property('help_at_or_below_squad_count', 1)
    net.set_editor_property('target_squad_count', 3)
    net.set_editor_property('stop_when_mission_class_completed',
                            unreal.load_asset(CLEAR_SOVIET_RIG).generated_class())
    net.set_editor_property('landing_points', [unreal.Vector(*p) for p in LANDING_POINTS])
    net.set_editor_property('parachute_ability', ability_enum().GA_GER_PARATROOPERS)
    manager.set_editor_property('player_squad_safety_net', net)
    got = manager.get_editor_property('player_squad_safety_net')
    print('safety net', got.get_editor_property('enabled'), got.get_editor_property('squad_subtype'),
          got.get_editor_property('help_at_or_below_squad_count'), got.get_editor_property('target_squad_count'),
          got.get_editor_property('stop_when_mission_class_completed'), len(got.get_editor_property('landing_points')))
    return manager
