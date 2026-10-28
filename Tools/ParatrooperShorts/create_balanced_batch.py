"""Prepare three independent, roughly matched five-tank paratrooper battles."""
import copy
import json
import sys

from create_german_preset import JOBS, PROJECT, point, unit

sys.path.insert(0, str(PROJECT / 'ShortsCreator/Tools'))
import dashboard_model
import forge_rounds


def vehicle(relative):
    return unit('GroundVehicles/TrackBasedVehicle/' + relative)


def build_jobs():
    base = json.loads((JOBS / 'GermanTwinBomberParatrooperBattle.json').read_text(encoding='utf-8-sig'))
    t85 = vehicle('Rus_Medium/T34/Rus_Vehicle_T3485')
    t85_heat = vehicle('Rus_Medium/T34/Rus_Vehicle_T3485_Soviet')
    t34e = vehicle('Rus_Medium/T34/Rus_Vehicle_T34E')
    t70_apcr = vehicle('Rus_Light/T70/Rus_Vehicle_T-70_Soviet')
    is1_heat = vehicle('Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-1_Soviet')
    is2 = vehicle('Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-2')
    su85_apcr = vehicle('Rus_Medium/SU85/Rus_Vehicle_Su85_Soviet')
    panther_d = vehicle('Ger_Heavy/PantherD/Ger_Vehicle_PantherD')
    panther_g = vehicle('Ger_Heavy/PantherG/Ger_Vehicle_PantherG')
    pz4g = vehicle('Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G')
    pz4h = vehicle('Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H')
    tiger = vehicle('Ger_Heavy/Tiger/Ger_Vehicle_TigerH1')
    stug = vehicle('Ger_Medium/Stug/Ger_Vehicle_StugIII')
    pz3 = vehicle('Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J')
    mosin = unit('Squads/Rus/Rus_Squad_Mosin')
    ptrs = unit('Squads/Rus/Rus_Squad_MosinPTRS')
    bazooka = unit('Squads/Rus/Rus_Squad_HazmatBazooka')
    scav = unit('Squads/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers')
    grenadier = unit('Squads/Ger/T2/PanzerGrenadiers/Squad_Ger_PanzerGrenadiers')
    bs3 = unit('Buildings/BuildingExpansions/RusBS3Pit/Rus_BXP_BS3Pit')
    light_at = unit('Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit')
    pak40 = unit('Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Pak40')
    flak88 = unit('Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Flak88')
    labels = dict(zip(
        [t85, t85_heat, t34e, t70_apcr, is1_heat, is2, su85_apcr, panther_d, panther_g, pz4g, pz4h,
         tiger, stug, pz3, mosin, ptrs, bazooka, scav, grenadier, bs3, light_at, pak40, flak88],
        ['T-34-85', 'T-34-85', 'T-34E', 'T-70', 'IS-1', 'IS-2', 'SU-85', 'Panther D', 'Panther G',
         'Panzer IV G', 'Panzer IV H', 'Tiger H1', 'StuG III', 'Panzer III J', 'Mosin squad',
         'Mosin PTRS squad', 'Hazmat bazooka squad', 'Scavenger squad', 'Panzergrenadier squad',
         'BS-3 (100 mm) pit', '53-K (45 mm) pit', 'PaK 40 pit', 'FlaK 88 pit']))
    recipes = [
        dict(name='ParatrooperFair01_GermanVultures_Mediums', owner=1, squad='Squad_Ger_Vultures',
             soviet=[t85] * 3 + [t85_heat] * 2, german=[panther_d] * 3 + [pz4g] * 2,
             soviet_infantry=[ptrs] * 2 + [mosin] * 2, german_infantry=[scav] * 2,
             soviet_pit=bs3, german_pit=pak40,
             caption1='SOVIETS | 3× T-34-85 | 2× T-34-85 | 2× Mosin PTRS squads | 2× Mosin squads | 1× BS-3 (100 mm) pit',
             caption2='GERMANS | 3× Panther D | 2× Panzer IV G | 2× Scavenger squads | 1× PaK 40 pit | 2× Vulture drops | 2× Ju 390 bombers'),
        dict(name='ParatrooperFair02_SovietMosin_HeavyArmour', owner=2, squad='Squad_Rus_Mosin',
             soviet=[t85] * 2 + [is1_heat, is2, su85_apcr], german=[tiger] * 2 + [panther_g] * 2 + [pz4h],
             soviet_infantry=[bazooka] * 2, german_infantry=[grenadier] * 2 + [scav] * 2,
             soviet_pit=bs3, german_pit=flak88,
             caption1='SOVIETS | 2× T-34-85 | 1× IS-1 | 1× IS-2 | 1× SU-85 | 2× Hazmat bazooka squads | 1× BS-3 (100 mm) pit | 2× Mosin drops | 2× Pe-8 transports',
             caption2='GERMANS | 2× Tiger H1 | 2× Panther G | 1× Panzer IV H | 2× Panzergrenadier squads | 2× Scavenger squads | 1× FlaK 88 pit'),
        dict(name='ParatrooperFair03_SovietROKS_Assault', owner=2, squad='Squad_Rus_ROKS3_PPSH',
             soviet=[t34e] * 3 + [t70_apcr] * 2, german=[pz3] * 3 + [stug] * 2,
             soviet_infantry=[ptrs] * 2, german_infantry=[grenadier] * 2 + [scav] * 2,
             soviet_pit=light_at, german_pit=pak40,
             caption1='SOVIETS | 3× T-34E | 2× T-70 | 2× Mosin PTRS squads | 1× 53-K (45 mm) pit | 2× ROKS assault drops | 2× Pe-8 transports',
             caption2='GERMANS | 3× Panzer III J | 2× StuG III | 2× Panzergrenadier squads | 2× Scavenger squads | 1× PaK 40 pit'),
    ]
    jobs = []
    for recipe in recipes:
        job = copy.deepcopy(base)
        job['JobName'] = recipe['name']
        segment = job['Segments'][0]
        segment.update(Subtype=recipe['name'], ClassPath=recipe['soviet'][0],
            BattleAllyClassPaths=recipe['soviet'][1:] + recipe['soviet_infantry'] + [recipe['soviet_pit']],
            BattleEnemyClassPaths=recipe['german'] + recipe['german_infantry'] + [recipe['german_pit']],
            BattleAllyOffsets=[point(0, -1500), point(0, 1500), point(-1200, -850), point(-1200, 850)]
                + infantry_positions(len(recipe['soviet_infantry']), 800) + [point(-2600)],
            BattleEnemyOffsets=[point(0, y) for y in [-1500, 0, 1500]]
                + [point(1200, y) for y in [-850, 850]]
                + infantry_positions(len(recipe['german_infantry']), -850) + [point(2800)],
            Caption1=recipe['caption1'], Caption2=recipe['caption2'],
            ParatrooperAbility='GA_GerParatroopers' if recipe['owner'] == 1 else 'GA_SovietParatroopers')
        german_drop = recipe['owner'] == 1
        segment['ParatrooperFormation'] = dict(
            OwningPlayer=recipe['owner'], LandingTrailLength=7200,
            JumpIntervalSeconds=.8 if german_drop else .48, SquadSubtypes=[recipe['squad']],
            Passes=[dict(DropOffset=point(7200 if german_drop else -3200, y),
                         AircraftStartOffset=point(500 if german_drop else -500, 0, altitude))
                    for y, altitude in [(-900, 3500), (900, 4300)]])
        classes = set([segment['ClassPath']] + segment['BattleAllyClassPaths'] + segment['BattleEnemyClassPaths'])
        job['UnitLabels'] = {class_path: labels[class_path] for class_path in classes}
        for class_path in classes:
            asset = PROJECT / 'Content' / class_path.split('.')[0].removeprefix('/Game/')
            assert asset.with_suffix('.uasset').exists(), class_path
        forge_rounds.annotate_job(job, str(PROJECT))
        resolved = dashboard_model.compose(PROJECT / 'ShortsCreator',
            {'jobId': 'GermanTwinBomberParatrooperBattle', 'setupId': 'ParatrooperBattle', 'advanced': job})
        assert not resolved['warnings'], resolved['warnings']
        jobs.append(job)
    return jobs


def infantry_positions(count, x):
    return [point(x, y) for y in ([-1000, 1000] if count == 2 else [-1650, -550, 550, 1650])]


def main():
    jobs = build_jobs()
    for job in jobs:
        destination = JOBS / (job['JobName'] + '.json')
        assert not destination.exists(), 'Do not overwrite an existing production preset: ' + str(destination)
        destination.write_text(json.dumps(job, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
        print(destination)


if __name__ == '__main__':
    main()
