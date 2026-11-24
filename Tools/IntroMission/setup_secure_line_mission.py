"""Makes Mission_Intro_ClearSovietRig the "Secure the line" mission (USecureBaseMission) and fills its class defaults.

Run inside the editor (exec the file over the UECP bridge or `py` in the editor console), then compile + save only
the mission Blueprint. Re-runnable: it reparents once and overwrites the defaults every time. The map is NOT touched:
the map's mission instance (BP_MissionManager.Missions[1]) has no overrides for these new properties, so it takes
them from the class defaults when the map loads.

Layout (Ger_IntroMission, Soviet base on the ridge plateau, ground z ~532-537, walls x -2610..4139, y -4246..5238):
  targets      both Rus_Building_CoolingTowers_Concrete, both Rus_Building_Barracks_Concrete, both Rus_BXP_GuardTowerMG
               and Rus_BXP_LongCamoBunker (7) - matched by parent class inside the base box.
  marching     4 Mosin squads + 1 PTRS squad on navmesh-checked lanes (2026-11-16 survey, straight paths).
  fuel truck   Rus_ArmoredCar_ZilFuel spawns behind the crest south of the base's rear (4640, -5200), 17 s into the tour,
               and drives straight north along the outside of the east wall, so it parks parallel to the wall next to
               the long bunker (yaw 90, stops ~1.5 m short of its target) where BP_MissionFuelCrane can reach its bed.
  tour         5 shots, about 30 s, then 2 s back to the player's view.
"""
import unreal

MISSION_BP = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/Mission_Intro_ClearSovietRig'
PARENT = '/Script/RTS_Survival.SecureBaseMission'
GROUND = 540.0


def cls(path):
    loaded = unreal.load_class(None, path)
    if loaded is None:
        raise RuntimeError('missing class ' + path)
    return loaded


def v(x, y, z=GROUND):
    return unreal.Vector(float(x), float(y), float(z))


def text(value):
    return unreal.Text(value)


TARGET_CLASSES = [
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_CoolingTowers.Rus_Building_CoolingTowers_C',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_Barracks.Rus_Building_Barracks_C',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_GuardTowerMG.Rus_BXP_GuardTowerMG_C',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_LongCamoBunker.Rus_BXP_LongCamoBunker_C',
]
MARKER = '/Game/RTS_Survival/Blueprints/Missions/Hints/BP_Hint_DestroyTarget.BP_Hint_DestroyTarget_C'
MOSIN = '/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C'
PTRS = '/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_PTRS.Rus_Squad_PTRS_C'
TRUCK = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilCargo/Rus_ArmoredCar_ZilFuel.Rus_ArmoredCar_ZilFuel_C'
CRANE = '/Game/RTS_Survival/Blueprints/Missions/MissionProps/FuelCrane/BP_MissionFuelCrane.BP_MissionFuelCrane_C'
TOUR_LINE = ('/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/IntroMission/'
             'IntroMission_SecureLineTour')
TOUR_LINE_SOURCE = (r'D:\Blender\BlenderProjects\BlenderTanks\VoiceLines\Announcer\AAAGerMissions\IntroMission'
                    r'\IntroMission_SecureLineTour.wav')

# (squad class, spawn xy, spawn yaw, march-to xy)
SQUADS = [
    (MOSIN, (-1900, -3300), 0.0, (500, -3300)),     # parade lane west of the barracks
    (MOSIN, (900, -1500), 0.0, (3000, -1500)),      # between the barracks and the fuel storage
    (MOSIN, (150, -1300), 90.0, (150, 2300)),       # behind the west wall and the main gate
    (MOSIN, (400, 4500), 0.0, (2500, 4500)),        # north lane behind the cooling towers
    (PTRS, (3750, 900), -90.0, (3750, -500)),       # east lane between cooling towers 2 and the rear wall
]

FUEL = {
    'truck_spawn': (4640, -5200, 850), 'truck_yaw': 90.0, 'truck_delay': 17.0,
    'park': (4640, -1580, 531), 'park_yaw': 90.0, 'parked_radius': 400.0,
    'crane': (3650, -1850, 532), 'crane_yaw': 0.0,
}

REVEAL = [(-1500, -2800), (-1300, 800), (900, -2600), (900, 1200), (900, 4300), (3200, -2400), (3200, 700),
          (3200, 3600), (5200, -1800)]

# (focus start, focus end, yaw start, yaw end, pitch start, pitch end, arm start, arm end, seconds, blend, title, sub)
SHOTS = [
    ((-2600, -200), (600, 200), 10, 0, 22, 18, 5200, 4400, 6.5, 1.8,
     'Soviet Ridge Base', 'The last Soviet hold on the ridgeline'),
    ((-1750, -2150), (-1350, -1550), 35, 50, 18, 14, 2600, 2300, 5.5, 1.5,
     'MG Guard Towers', 'Machine-gun nests watch the gates'),
    ((1300, -2700), (2500, -2900), 0, -10, 15, 15, 2800, 2600, 5.5, 1.5,
     'Barracks', 'Quarters of the base garrison'),
    ((1000, 3100), (1300, 3300), 60, 25, 16, 14, 3200, 2900, 4.5, 1.4,
     'Cooling Towers', 'They keep the base generators running'),
    ((2600, -200), (2950, 150), -20, -45, 16, 14, 3100, 2800, 4.5, 1.4,
     'Cooling Towers', 'Two of them: both must fall'),
    ((4700, -2400), (4050, -1800), 190, 165, 20, 18, 2700, 2300, 7.5, 1.6,
     'Long Camo Bunker', 'Fuel for the garrison comes in at the rear'),
]


def apply_defaults(cdo):
    cdo.set_editor_property('target_classes', [cls(p) for p in TARGET_CLASSES])
    cdo.set_editor_property('target_area_centre', v(750, 500, 0))
    cdo.set_editor_property('target_area_half_extent', v(3500, 4900, 0))
    cdo.set_editor_property('target_marker_class', cls(MARKER))
    cdo.set_editor_property('target_marker_text', text('Destroy'))
    cdo.set_editor_property('target_marker_height_above_target', 150.0)
    cdo.set_editor_property('target_mini_map_icon', unreal.MinimapIconType.ATTACK_TARGET)
    cdo.set_editor_property('objective_title', text('Secure the Line'))
    cdo.set_editor_property('objective_description', text(
        'Destroy the Soviet ridge base: its cooling towers, barracks, MG guard towers and the long camo bunker. '
        '{Destroyed}/{Total} destroyed.'))
    cdo.set_editor_property('reveal_centres', [v(x, y) for x, y in REVEAL])
    cdo.set_editor_property('reveal_radius', 2400.0)
    cdo.set_editor_property('reveal_seconds', 60.0)

    squads = []
    for path, spawn, yaw, march in SQUADS:
        squad = unreal.SecureBaseMarchingSquad()
        squad.set_editor_property('squad_class', cls(path))
        squad.set_editor_property('spawn_location', v(spawn[0], spawn[1], 600))
        squad.set_editor_property('spawn_yaw', yaw)
        squad.set_editor_property('march_to_location', v(march[0], march[1]))
        squads.append(squad)
    cdo.set_editor_property('marching_squads', squads)

    fuel = unreal.SecureBaseFuelDelivery()
    fuel.set_editor_property('enabled', True)
    fuel.set_editor_property('truck_class', cls(TRUCK))
    fuel.set_editor_property('truck_spawn_location', v(*FUEL['truck_spawn']))
    fuel.set_editor_property('truck_spawn_yaw', FUEL['truck_yaw'])
    fuel.set_editor_property('truck_spawn_delay_seconds', FUEL['truck_delay'])
    fuel.set_editor_property('park_location', v(*FUEL['park']))
    fuel.set_editor_property('park_yaw', FUEL['park_yaw'])
    fuel.set_editor_property('parked_radius', FUEL['parked_radius'])
    crane = unreal.load_class(None, CRANE)
    if crane is None:
        unreal.log_warning('BP_MissionFuelCrane not there yet; crane class left empty')
    fuel.set_editor_property('crane_class', crane)
    fuel.set_editor_property('crane_location', v(*FUEL['crane']))
    fuel.set_editor_property('crane_yaw', FUEL['crane_yaw'])
    cdo.set_editor_property('fuel_delivery', fuel)

    shots = []
    for (fs, fe, ys, ye, ps, pe, a_s, a_e, secs, blend, title, sub) in SHOTS:
        shot = unreal.MissionCameraTourShot()
        shot.set_editor_property('focus_start', v(*fs))
        shot.set_editor_property('focus_end', v(*fe))
        shot.set_editor_property('yaw_start', float(ys))
        shot.set_editor_property('yaw_end', float(ye))
        shot.set_editor_property('pitch_start', float(ps))
        shot.set_editor_property('pitch_end', float(pe))
        shot.set_editor_property('arm_length_start', float(a_s))
        shot.set_editor_property('arm_length_end', float(a_e))
        shot.set_editor_property('seconds', float(secs))
        shot.set_editor_property('blend_in_seconds', float(blend))
        shot.set_editor_property('title', text(title))
        shot.set_editor_property('subtitle', text(sub))
        shots.append(shot)
    cdo.set_editor_property('tour_shots', shots)
    settings = unreal.MissionCameraTourSettings()
    settings.set_editor_property('show_letterbox', True)
    settings.set_editor_property('letterbox_height_fraction', 0.1)
    settings.set_editor_property('return_seconds', 2.0)
    cdo.set_editor_property('tour_settings', settings)
    cdo.set_editor_property('max_seconds_waiting_for_base_life', 12.0)
    # Advisor over the tour (user pick 2026-11-16, take 04 of 8): starts 0.8 s after the first shot shows.
    line = unreal.load_asset(TOUR_LINE)
    if line is None:
        raise RuntimeError('missing ' + TOUR_LINE + ': import IntroMission_SecureLineTour.wav first '
                           '(secure_line_headless_test.ensure_tour_line)')
    cdo.set_editor_property('tour_voice_line', line)
    cdo.set_editor_property('tour_voice_line_portrait', unreal.RTSPortraitTypes.GERMAN_ANNOUNCER)
    cdo.set_editor_property('tour_voice_line_delay_seconds', 0.8)


def run():
    bp = unreal.load_asset(MISSION_BP)
    parent = unreal.load_class(None, PARENT)
    if parent is None:
        raise RuntimeError('USecureBaseMission is not in this editor build')
    current = unreal.get_default_object(bp.generated_class())
    if not isinstance(current, unreal.SecureBaseMission):
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent)
        unreal.log_warning(f'reparented {MISSION_BP} to SecureBaseMission')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    apply_defaults(cdo)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.log_warning('secure line defaults applied')
    return bp


if __name__ == '__main__':
    run()
