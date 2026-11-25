"""Builds Mission_Berlin_SupplyPoint (BerlinMissionScripting.md section 11): BONUS "Take the supply point", started 140 s
in by a UTimeElapsedMissionTrigger (wire_berlin_stages.py). Already held when the plate appears: reward + complete at
once. Otherwise a skippable camera tour of the crane with the Advisor's explanation (Berlin_Cinematic_SupplyPoint), then
a capture check every CaptureCheckSeconds; held: reward (three Sd.Kfz. 221 flown in at the HQ) + complete.
Usage: python build_supply_point_stage.py <port>     (the event graph is laid out by layout_eventgraphs.py)
"""
import sys

from uecp_client import action

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'
BP = BASE + 'Mission_Berlin_SupplyPoint'
LINE = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/Berlin/Berlin_Cinematic_SupplyPoint'
PLAYER = 1
MANAGER = {'op': 'call', 'func': 'Self.GetMissionManagerChecked'}

VARIABLES = [
    {'variable_name': 'SupplyPoint', 'variable_type': 'object:SupplyPoint', 'category': 'Berlin|State'},
    {'variable_name': 'IntroLine', 'variable_type': 'object:SoundBase', 'instance_editable': True,
     'category': 'Berlin|Lines', 'tooltip': 'The Advisor explains supply points over the camera tour.'},
    {'variable_name': 'RewardPayload', 'variable_type': 'struct:PlayerAirDropPayload', 'instance_editable': True,
     'category': 'Berlin|Reward', 'tooltip': 'Flown in at the HQ when the point is held (three Sd.Kfz. 221).'},
    {'variable_name': 'RewardLandingOffset', 'variable_type': 'vector', 'default_value': '(X=1800,Y=0,Z=0)',
     'instance_editable': True, 'category': 'Berlin|Reward',
     'tooltip': 'From the HQ to where the reward lands, so it lands inside the base but beside the HQ.'},
    {'variable_name': 'CaptureCheckSeconds', 'variable_type': 'int', 'default_value': '1', 'instance_editable': True,
     'category': 'Berlin|Timing', 'tooltip': 'Seconds between checks whether the player holds the point.'},
    {'variable_name': 'CaptureCheckTaskId', 'variable_type': 'int', 'default_value': '-1', 'category': 'Berlin|State'},
]

FIND = {
    'function': 'FindSupplyPoint',
    'outputs': [{'name': 'bFound', 'type': 'bool'}],
    'body': [
        {'op': 'call', 'func': 'GameplayStatics.GetAllActorsOfClass',
         'args': {'WorldContextObject': MANAGER, 'ActorClass': '/Script/RTS_Survival.SupplyPoint'}, 'as': 'Points'},
        {'op': 'branch', 'condition': '=len($Points) > 0', 'then': [
            {'op': 'cast', 'value': '=$Points[0]', 'to': '/Script/RTS_Survival.SupplyPoint', 'as': 'Point', 'then': [
                {'op': 'set_var', 'var': 'SupplyPoint', 'value': '$Point'},
                {'op': 'return', 'values': {'bFound': True}}]}]},
        {'op': 'return', 'values': {'bFound': False}},
    ],
}

IS_HELD = {
    'function': 'IsSupplyPointHeld',
    'outputs': [{'name': 'bHeld', 'type': 'bool'}],
    'body': [
        {'op': 'is_valid', 'value': '$SupplyPoint', 'then': [
            {'op': 'return', 'values': {'bHeld': f'=$SupplyPoint.GetOwningPlayer() == {PLAYER}'}}]},
        {'op': 'return', 'values': {'bHeld': False}},
    ],
}


def shot(name, focus_start, focus_end, yaw_start, yaw_end, seconds, title, subtitle):
    return {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': name, 'fields': {
        'FocusStart': focus_start, 'FocusEnd': focus_end, 'YawStart': yaw_start, 'YawEnd': yaw_end,
        'Seconds': seconds, 'BlendInSeconds': 1.5, 'RevealRadius': 3500,
        'Title': {'op': 'format_text', 'format': title}, 'Subtitle': {'op': 'format_text', 'format': subtitle}}}


TOUR = {
    'function': 'PlaySupplyPointTour',
    'body': [
        {'op': 'call', 'func': 'Actor.K2_GetActorLocation', 'target': '$SupplyPoint', 'as': 'PointLocation'},
        # 28.3 s line: 9.5 + 9 + 10 s of shots.
        shot('CraneShot', '=$PointLocation + MakeVector(-2500, -2500, 0)', '$PointLocation', -60, -30, 9.5,
             'Supply point', 'Hold the area until our flag is raised'),
        shot('TruckShot', '$PointLocation', '=$PointLocation + MakeVector(600, 300, 0)', -30, 10, 9,
             'The supply truck', 'The crane loads it; the HQ is paid when it arrives'),
        shot('RoadShot', '$PointLocation', '=$PointLocation + MakeVector(9000, 1700, 0)', 10, 40, 10,
             'Keep the road safe', 'Supply trucks cannot be commanded'),
        {'op': 'make_array', 'as': 'Shots', 'items': ['$CraneShot', '$TruckShot', '$RoadShot']},
        {'op': 'make_struct', 'struct': 'MissionCameraTourSettings', 'as': 'TourSettings',
         'fields': {'StartSound': '$IntroLine', 'bAllowSkipping': True}},
        {'op': 'call', 'func': 'Self.PlayCameraTour', 'args': {'Shots': '$Shots', 'Settings': '$TourSettings',
                                                                'ID': 1}},
    ],
}

REWARD = {
    'function': 'GiveSupplyPointReward',
    'body': [
        {'op': 'call', 'func': 'Self.GetPlayerNomadicsOfType', 'args': {'NomadicSubtype': 'Nomadic_GerHq'},
         'as': 'HQs'},
        {'op': 'branch', 'condition': '=len($HQs) > 0', 'then': [
            {'op': 'call', 'func': 'Actor.K2_GetActorLocation', 'target': '=$HQs[0]', 'as': 'HQLocation'},
            {'op': 'call', 'func': 'PlayerAirDropSubsystem.GetPlayerAirDropSubsystem',
             'args': {'WorldContextObject': MANAGER}, 'as': 'AirDrop'},
            {'op': 'is_valid', 'value': '$AirDrop', 'then': [
                {'op': 'call', 'func': 'PlayerAirDropSubsystem.DeliverPlayerUnitsByAir', 'target': '$AirDrop',
                 'args': {'LandingPoint': '=$HQLocation + $RewardLandingOffset', 'Payload': '$RewardPayload'}},
                {'op': 'call', 'func': 'Self.ShowReinforcementsOnMiniMap',
                 'args': {'WorldLocation': '=$HQLocation + $RewardLandingOffset', 'DurationSeconds': 8}}]}]},
    ],
}


def main():
    port = int(sys.argv[1])
    r = action(port, 'create_asset', {'asset_type': 'blueprint', 'name': 'Mission_Berlin_SupplyPoint',
                                      'save_path': BASE[:-1], 'parent_class': '/Script/RTS_Survival.MissionBase'})
    print('create', r.get('ok'), str(r.get('error', ''))[:200])
    for item in VARIABLES:
        r = action(port, 'add_variable', dict(item, blueprint_path=BP))
        print('var', item['variable_name'], r.get('ok'), r.get('variable_type'), r.get('error', ''))
    for name, value in (('IntroLine', LINE + '.Berlin_Cinematic_SupplyPoint'),
                        ('RewardPayload', '(Vehicles=(Tank_Sdkfz_221,Tank_Sdkfz_221,Tank_Sdkfz_221),Squads=,'
                                          'bParachuteSquads=True,CargoPlaneAbility=GA_GerPzIIIFlak41Drop,'
                                          'ParachuteAbility=GA_GerParatroopers)')):
        r = action(port, 'set_blueprint_variable_default', {'blueprint_path': BP, 'variable_name': name,
                                                            'default_value': value})
        print('default', name, r.get('ok'), str(r.get('error', ''))[:300])
    for spec in (FIND, IS_HELD, TOUR, REWARD):
        r = action(port, 'synthesize_graph', dict(spec, blueprint=BP))
        print(spec['function'], r.get('ok'), str(r.get('error', ''))[:900].encode('ascii', 'replace').decode())
    c = action(port, 'compile_blueprint', {'blueprint_path': BP})
    print('compile', c.get('error_count'), str(c.get('errors', ''))[:900])


if __name__ == '__main__':
    main()
