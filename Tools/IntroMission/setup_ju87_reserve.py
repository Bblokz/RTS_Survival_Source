"""Ger_IntroMission, user request 2026-11-22: Okhotnik intel + the Ju 87 reserve after the "Secure the Line" strike.

On the Mission_Intro_ClearSovietRig class defaults (USecureBaseMission assault response):
  - the strike flies 4 Ju 87s (was 2);
  - 8 s after the strike takes off, the Okhotnik (PTRS) squad info takeover opens for a squad that dropped into the base
    (row "Okhotnik" of DT_SquadInfoTakeovers, written by Tools/InfoTakeover/author_squad_info_takeovers.py);
  - 2026-11-25: right after it, the crushing field manual (walls, then running over infantry; UW_WallBreachTakeover)
    opens (bShowWallBreachManualAfterIntelSquad: queued behind the Okhotnik screen);
  - 60 s after the strike takes off, the player gets a Ju 87 strike of their own (4 aircraft that keep attacking the
    targeted area) with 2 charges, a 180 s cooldown and no cost, and the announcer plays IntroMission_Ju87Reserve.
On the map: the mission manager hides the player's global abilities (bHideGlobalAbilitiesForPlayer), so the panel only
appears once the reserve is granted and disappears again after the second use.

Exec in a throwaway editor with the map open (rear_editor_server / rear_cmd, after rear_lib.py). Call
apply_mission() then save the Blueprint; apply_map() (hide flag + the same values on the map's mission instance) then
save the map once the navmesh is built.
"""
import unreal

MISSION_BP = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/Mission_Intro_ClearSovietRig'
LINES = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/IntroMission/'
RESERVE_LINE = LINES + 'IntroMission_Ju87Reserve'

STRIKE_AIRCRAFT = 4
INTEL_SQUAD_ROW = 'Okhotnik'
INTEL_DELAY_SECONDS = 8.0
WALL_MANUAL_AFTER_INTEL = True
GRANT_DELAY_SECONDS = 60.0
GRANT_CHARGES = 2
GRANT_COOLDOWN_SECONDS = 180
GRANT_AIRCRAFT = 4


def ability_enum():
    """EGlobalAbility shares its Python name with the UGlobalAbility class: take the enum type from a field value."""
    return type(unreal.EnemyAirDropDeliverySettings().get_editor_property('cargo_plane_ability'))


def apply_to(mission):
    """Writes the values on one USecureBaseMission (the Blueprint's class defaults or the map's instance)."""
    resp = mission.get_editor_property('assault_response')
    resp.set_editor_property('air_support_aircraft_count', STRIKE_AIRCRAFT)
    resp.set_editor_property('intel_squad_subtype', unreal.SquadSubtype.SQUAD_RUS_OKHOTNIK)
    resp.set_editor_property('intel_squad_library_entry', INTEL_SQUAD_ROW)
    resp.set_editor_property('intel_squad_delay_seconds', INTEL_DELAY_SECONDS)
    resp.set_editor_property('show_wall_breach_manual_after_intel_squad', WALL_MANUAL_AFTER_INTEL)
    grant = unreal.MissionGrantedGlobalAbility()
    grant.set_editor_property('ability', ability_enum().GA_JU87_STRAFING)
    grant.set_editor_property('charges', GRANT_CHARGES)
    grant.set_editor_property('cooldown_seconds', GRANT_COOLDOWN_SECONDS)
    grant.set_editor_property('free_of_cost', True)
    grant.set_editor_property('strafe_aircraft_count', GRANT_AIRCRAFT)
    grant.set_editor_property('strafe_stays_on_target', True)
    resp.set_editor_property('granted_ability', grant)
    resp.set_editor_property('granted_ability_delay_seconds', GRANT_DELAY_SECONDS)
    line = unreal.load_asset(RESERVE_LINE) if unreal.EditorAssetLibrary.does_asset_exist(RESERVE_LINE) else None
    if line is None:
        unreal.log_warning('ju87 reserve: no ' + RESERVE_LINE + ' yet; the grant plays no line')
    resp.set_editor_property('granted_ability_line', line)
    resp.set_editor_property('granted_ability_portrait', unreal.RTSPortraitTypes.GERMAN_ANNOUNCER)
    mission.set_editor_property('assault_response', resp)
    print(mission.get_name(), 'aircraft', resp.get_editor_property('air_support_aircraft_count'),
          'intel', resp.get_editor_property('intel_squad_library_entry'),
          'wall manual', resp.get_editor_property('show_wall_breach_manual_after_intel_squad'),
          'grant', resp.get_editor_property('granted_ability').get_editor_property('ability'),
          'line', line.get_path_name() if line else None)


def apply_mission(compile_bp=True):
    bp = unreal.load_asset(MISSION_BP)
    apply_to(unreal.get_default_object(bp.generated_class()))
    if compile_bp:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp


def map_clear_missions(manager):
    """The ClearSovietRig instances in the map's mission chain (an instance keeps its own copy of the values)."""
    found = []
    for top in manager.get_editor_property('missions'):
        while top is not None:
            if top.get_class().get_name() == 'Mission_Intro_ClearSovietRig_C':
                found.append(top)
            top = top.get_editor_property('next_mission')
    return found


def apply_map():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    managers = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MissionManager)]
    if len(managers) != 1:
        raise RuntimeError('expected one MissionManager, found %d' % len(managers))
    manager = managers[0]
    manager.set_editor_property('hide_global_abilities_for_player', True)
    print('map: hide global abilities', manager.get_editor_property('hide_global_abilities_for_player'))
    clears = map_clear_missions(manager)
    if not clears:
        raise RuntimeError('no Mission_Intro_ClearSovietRig in the map mission chain')
    for clear in clears:
        apply_to(clear)
    return manager
