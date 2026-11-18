"""Fills the "assault response" class defaults of Mission_Intro_ClearSovietRig (USecureBaseMission), 2026-11-16.

When a player unit comes up the west ramp onto the plateau in front of the Soviet ridge base (the 53-K sector pit at
(-2184,-506) and the dug-in T-34E at (-1200,989) cover it), after the "Secure the Line" camera tour:
  1. the Mosin rifleman shouts IntroMission_MosinBaseUnderAttack on the SovietMosin portrait,
  2. two Soviet paratrooper transports drop Mosin + PTRS squads inside the base and a cargo plane lowers a T-26 on
     ropes between the barracks and the fuel storage,
  3. the advisor plays IntroMission_StukaSupport and two Ju 87s keep strafing the base for 60 s (every run over the base
     centre, no free hunting).

Run inside an editor whose RTS_Survival module has FSecureBaseAssaultResponse (exec over UECP or `py` in the console),
then compile + save only the mission Blueprint. Separate from setup_secure_line_mission.py (re-running that one does
not touch these values). Re-runnable.

Ramp survey (2026-11-16 height grid): ground ~140 at x -6800 rising to the 590 plateau at x -4400, y -1600..1600.
Landing spots: >= 7 m clear of every structure taller than the floor slabs (actor-bounds survey).
"""
import unreal

MISSION_BP = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/Mission_Intro_ClearSovietRig'
LINES = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/IntroMission/'
MOSIN_LINE = LINES + 'IntroMission_MosinBaseUnderAttack'
STUKA_LINE = LINES + 'IntroMission_StukaSupport'

TRIGGER_CENTRE = (-4300.0, 0.0, 500.0)      # upper ramp + plateau edge
TRIGGER_HALF = (900.0, 1900.0, 700.0)       # x -5200..-3400, y -1900..1900
PARA_DROPS = [((300.0, -1750.0, 540.0), 2.0), ((1900.0, 4450.0, 540.0), 4.0)]   # (landing point, delay)
T26_DROP = ((1950.0, -1550.0, 540.0), 3.0)
STRIKE_TARGET = (800.0, 500.0, 540.0)       # base centre


def ability_enum():
    """EGlobalAbility shares its Python name with the UGlobalAbility class: take the enum type from a field value."""
    return type(unreal.EnemyAirDropDeliverySettings().get_editor_property('cargo_plane_ability'))


def vec(t):
    return unreal.Vector(*t)


def squad(subtype):
    unit = unreal.EnemyPlacementUnit()
    unit.set_editor_property('kind', unreal.EnemyPlacementUnitKind.SQUAD)
    unit.set_editor_property('squad_subtype', subtype)
    return unit


def vehicle(subtype):
    unit = unreal.EnemyPlacementUnit()
    unit.set_editor_property('kind', unreal.EnemyPlacementUnitKind.VEHICLE)
    unit.set_editor_property('vehicle_subtype', subtype)
    return unit


def entry(unit, count=1):
    e = unreal.EnemyAirDropUnitEntry()
    e.set_editor_property('unit_options', [unit])
    e.set_editor_property('count', count)
    return e


def delivery(kind, rope=False):
    d = unreal.EnemyAirDropDeliverySettings()
    d.set_editor_property('delivery', kind)
    d.set_editor_property('cargo_plane_ability', ability_enum().GA_SOVIET_T34_DROP)
    d.set_editor_property('parachute_ability', ability_enum().GA_SOVIET_PARATROOPERS)
    # The mission plays its own lines; the generic "paratroopers incoming" warning would talk over them.
    d.set_editor_property('warn_player', False)
    d.set_editor_property('rope_slung_vehicles', rope)
    return d


def drop(point, delay, units, deliv):
    r = unreal.SecureBaseResponseAirDrop()
    r.set_editor_property('landing_point', vec(point))
    r.set_editor_property('delay_seconds', delay)
    r.set_editor_property('units', units)
    r.set_editor_property('delivery', deliv)
    return r


def apply_defaults(cdo):
    mosin = squad(unreal.SquadSubtype.SQUAD_RUS_MOSIN)
    ptrs = squad(unreal.SquadSubtype.SQUAD_RUS_OKHOTNIK)    # Rus_Squad_PTRS
    drops = []
    for point, delay in PARA_DROPS:
        units = unreal.EnemyAirDropUnits()
        units.set_editor_property('base_units', [entry(mosin), entry(ptrs)])
        units.set_editor_property('extra_units_hard', [entry(mosin)])
        drops.append(drop(point, delay, units, delivery(unreal.EnemyAirDropDelivery.PARACHUTE)))
    t26 = unreal.EnemyAirDropUnits()
    t26.set_editor_property('base_units', [entry(vehicle(unreal.TankSubtype.TANK_T26))])
    drops.append(drop(T26_DROP[0], T26_DROP[1], t26, delivery(unreal.EnemyAirDropDelivery.CARGO_PLANE, rope=True)))

    resp = unreal.SecureBaseAssaultResponse()
    resp.set_editor_property('enabled', True)
    resp.set_editor_property('trigger_centre', vec(TRIGGER_CENTRE))
    resp.set_editor_property('trigger_half_extent', vec(TRIGGER_HALF))
    resp.set_editor_property('enemy_alarm_line', unreal.load_asset(MOSIN_LINE))
    resp.set_editor_property('enemy_alarm_portrait', unreal.RTSPortraitTypes.SOVIET_MOSIN)
    resp.set_editor_property('air_drops', drops)
    resp.set_editor_property('air_support_line_delay_seconds', 7.5)
    resp.set_editor_property('air_support_line', unreal.load_asset(STUKA_LINE))
    resp.set_editor_property('air_support_portrait', unreal.RTSPortraitTypes.GERMAN_ANNOUNCER)
    resp.set_editor_property('air_support_ability', ability_enum().GA_JU87_STRAFING)
    resp.set_editor_property('air_support_strike_delay_seconds', 9.0)
    resp.set_editor_property('air_support_aircraft_count', 2)
    resp.set_editor_property('air_support_strafe_seconds', 150.0)  # 2026-11-18 user: +90 s
    resp.set_editor_property('air_support_target', vec(STRIKE_TARGET))
    resp.set_editor_property('air_support_stays_on_target', True)   # runs keep going over the base
    cdo.set_editor_property('assault_response', resp)
    for path in (MOSIN_LINE, STUKA_LINE):
        if unreal.load_asset(path) is None:
            unreal.log_warning('assault response: missing sound ' + path)


def run(compile_bp=True):
    bp = unreal.load_asset(MISSION_BP)
    cdo = unreal.get_default_object(bp.generated_class())
    if not isinstance(cdo, unreal.SecureBaseMission):
        raise RuntimeError('Mission_Intro_ClearSovietRig is not a SecureBaseMission; run setup_secure_line_mission first')
    apply_defaults(cdo)
    if compile_bp:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.log_warning('assault response defaults applied')
    return bp


if __name__ == '__main__':
    run()
