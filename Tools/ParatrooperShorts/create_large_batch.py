"""Three large meadow recipes with six combat classes each, including every dropped squad.

Run from the RTS_Shorts Source tools folder. --queue submits each recipe once to its
own Studio; preflight and readable roster images are produced before rendering.
"""
import argparse
import copy
import json
from pathlib import Path
import sys
import urllib.request

PROJECT = Path(__file__).resolve().parents[3]
SHORTS = PROJECT / 'ShortsCreator'
sys.path.insert(0, str(SHORTS / 'Tools'))
import dashboard_model
import forge_rounds


def unit(relative):
    return '/Game/RTS_Survival/Blueprints/' + relative + '.' + relative.rsplit('/', 1)[-1] + '_C'


def vehicle(relative):
    return unit('GroundVehicles/TrackBasedVehicle/' + relative)


def point(x, y=0, z=0):
    return dict(X=float(x), Y=float(y), Z=float(z))


def tank_positions(count, soviet):
    # A centre lead and two staggered rows keep every gun clear of the next hull.
    forward_sign = -1 if soviet else 1
    front = [point(0, y) for y in [0, -1800, -900, 900, 1800]]
    rear_y = [-1350, 0, 1350] if count == 8 else [-1800, -900, 0, 900, 1800]
    return front + [point(forward_sign * 1400, y) for y in rear_y]


def infantry_positions(count, soviet):
    row_size = (count + 1) // 2
    sign = 1 if soviet else -1
    return [point(sign * (750 + (index // row_size) * 550),
                  -2100 + (index % row_size) * 4200 / max(1, row_size - 1))
            for index in range(count)]


def build_jobs():
    base = json.loads((SHORTS / 'Jobs/GermanTwinBomberParatrooperBattle.json').read_text(encoding='utf-8-sig'))
    panther_d = vehicle('Ger_Heavy/PantherD/Ger_Vehicle_PantherD')
    panther_g = vehicle('Ger_Heavy/PantherG/Ger_Vehicle_PantherG')
    pz4h = vehicle('Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H')
    pz4g = vehicle('Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G')
    tiger = vehicle('Ger_Heavy/Tiger/Ger_Vehicle_TigerH1')
    stug = vehicle('Ger_Medium/Stug/Ger_Vehicle_StugIII')
    t85_heat = vehicle('Rus_Medium/T34/Rus_Vehicle_T3485_Soviet')
    t34e = vehicle('Rus_Medium/T34/Rus_Vehicle_T34E')
    is1_heat = vehicle('Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-1_Soviet')
    is2 = vehicle('Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-2')
    su85_apcr = vehicle('Rus_Medium/SU85/Rus_Vehicle_Su85_Soviet')
    vulture = unit('Squads/Ger/ArmoryUnlocked/Squad_Ger_HazmatVultures')
    grenadier = unit('Squads/Ger/T2/PanzerGrenadiers/Squad_Ger_PanzerGrenadiers')
    mosin = unit('Squads/Rus/Rus_Squad_Mosin')
    roks = unit('Squads/Rus/AssaultSquads/Rus_Squad_FlameROKS')
    ptrs = unit('Squads/Rus/Rus_Squad_MosinPTRS')
    labels = dict(zip(
        [panther_d, panther_g, pz4h, pz4g, tiger, stug, t85_heat, t34e, is1_heat, is2,
         su85_apcr, vulture, grenadier, mosin, roks, ptrs],
        ['Panther D', 'Panther G', 'Panzer IV H', 'Panzer IV G', 'Tiger H1', 'StuG III',
         'T-34-85', 'T-34E', 'IS-1', 'IS-2', 'SU-85', 'Vulture squads',
         'Panzergrenadier sq.', 'Mosin squads', 'ROKS squads', 'Mosin PTRS sq.']))
    recipes = [
        dict(name='ParatrooperLarge01_Panthers_vs_HEAT', drop_owner=1,
             soviet_tanks=[t85_heat] * 6 + [is2] * 2, soviet_infantry=[mosin] * 8,
             german_tanks=[panther_d] * 5 + [pz4h] * 3, german_infantry=[vulture] * 4,
             drops={vulture: 2}),
        dict(name='ParatrooperLarge02_Soviet_HeavyAssault', drop_owner=2,
             soviet_tanks=[is1_heat] * 8, soviet_infantry=[mosin] * 6 + [roks] * 4,
             german_tanks=[tiger] * 4 + [panther_g] * 4, german_infantry=[grenadier] * 12,
             drops={mosin: 2, roks: 2}),
        dict(name='ParatrooperLarge03_Stugs_vs_APCR', drop_owner=1,
             soviet_tanks=[t34e] * 6 + [su85_apcr] * 4, soviet_infantry=[ptrs] * 10,
             german_tanks=[pz4g] * 6 + [stug] * 4, german_infantry=[vulture] * 6,
             drops={vulture: 2}),
    ]
    result = []
    for recipe in recipes:
        job = copy.deepcopy(base)
        job.update(JobName=recipe['name'], UnitOwningPlayer=2, CaptionScale=.92)
        job.pop('EditorialOverlayRecipe', None)
        segment = job['Segments'][0]
        soviet = recipe['soviet_tanks'] + recipe['soviet_infantry']
        german = recipe['german_tanks'] + recipe['german_infantry']
        segment.update(Subtype=recipe['name'], ClassPath=soviet[0],
            BattleAllyClassPaths=soviet[1:], BattleEnemyClassPaths=german,
            BattleAllyOffsets=tank_positions(len(recipe['soviet_tanks']), True)[1:]
                + infantry_positions(len(recipe['soviet_infantry']), True),
            BattleEnemyOffsets=tank_positions(len(recipe['german_tanks']), False)
                + infantry_positions(len(recipe['german_infantry']), False),
            ParatrooperAbility='GA_GerParatroopers' if recipe['drop_owner'] == 1 else 'GA_SovietParatroopers',
            SlotSeconds=120, StopAtSeconds=120, BattleHealthScale=1)
        german_drop = recipe['drop_owner'] == 1
        segment['ParatrooperFormation'] = dict(
            OwningPlayer=recipe['drop_owner'], LandingTrailLength=7200,
            JumpIntervalSeconds=.8 if german_drop else .48,
            Passes=[dict(DropOffset=point(7200 if german_drop else -3200, y),
                         AircraftStartOffset=point(500 if german_drop else -500, 0, altitude))
                    for y, altitude in [(-950, 3500), (950, 4300)]])
        combat_classes = set(soviet + german + list(recipe['drops']))
        assert len(combat_classes) == 6
        assert len(segment['BattleAllyOffsets']) == len(segment['BattleAllyClassPaths'])
        assert len(segment['BattleEnemyOffsets']) == len(segment['BattleEnemyClassPaths'])
        for class_path in combat_classes:
            asset = PROJECT / 'Content' / (class_path.split('.')[0].removeprefix('/Game/') + '.uasset')
            assert asset.is_file(), class_path
        job['UnitLabels'] = {class_path: labels[class_path] for class_path in sorted(combat_classes)}
        segment['Caption1'] = roster('SOVIETS', soviet, recipe['drops'] if not german_drop else {}, labels)
        segment['Caption2'] = roster('GERMANS', german, recipe['drops'] if german_drop else {}, labels)
        # The same production hook annotates every render, including these paratrooper jobs.
        forge_rounds.annotate_job(job, str(PROJECT))
        request = dict(jobId='GermanTwinBomberParatrooperBattle', name=job['JobName'],
                       setupId='', biome='source', advanced=job, post=True, keepFrames=False)
        resolved = dashboard_model.compose(SHORTS, request)
        assert not resolved['warnings'], resolved['warnings']
        result.append((job, recipe))
    return result


def roster(faction, ground_classes, drops, labels):
    from collections import Counter
    counts = Counter(ground_classes)
    counts.update(drops)
    assert len(counts) == 3
    return ' | '.join([faction] + [str(count) + '× ' + labels[class_path]
                                   for class_path, count in counts.items()])


def save_jobs(jobs):
    report = []
    for job, recipe in jobs:
        name = job['JobName']
        path = SHORTS / 'Jobs' / (name + '.json')
        assert not path.exists() or json.loads(path.read_text(encoding='utf-8')) == job
        path.write_text(json.dumps(job, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
        for ignore, prefix in [(PROJECT / '.gitignore', '!/ShortsCreator/Jobs/'), (SHORTS / '.gitignore', '!Jobs/')]:
            original = ignore.read_text(encoding='utf-8')
            exception = prefix + name + '.json'
            if exception not in original.splitlines():
                ignore.write_text(original.rstrip() + '\n' + exception + '\n', encoding='utf-8')
        report.append(dict(name=name, combat_types=6, drop_owner=recipe['drop_owner'],
            soviet_tanks=len(recipe['soviet_tanks']), german_tanks=len(recipe['german_tanks']),
            ground_squads=len(recipe['soviet_infantry']) + len(recipe['german_infantry']),
            dropped_squads=sum(recipe['drops'].values()), expected_drops=recipe['drops'],
            captions=[job['Segments'][0]['Caption1'], job['Segments'][0]['Caption2']]))
    return report


def queue_jobs(jobs, port):
    url = 'http://127.0.0.1:' + str(port)
    library = None
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url + '/api/library', timeout=30) as response:
                library = json.load(response)
            break
        except (ConnectionResetError, TimeoutError):
            if attempt == 2:
                raise
    assert Path(library['root']).resolve() == SHORTS.resolve(), 'Never submit to the other project Studio.'
    existing = json.loads((SHORTS / 'Dashboard/Data/queue.json').read_text(encoding='utf-8'))
    summary = []
    for job, recipe in jobs:
        name = job['JobName']
        prior = [item for item in existing if item['name'] == name]
        if prior:
            summary.append({key: prior[-1].get(key) for key in ['name', 'id', 'status']})
            continue
        request = dict(jobId=name, name=name, setupId='', biome='source', post=True, keepFrames=False)
        http_request = urllib.request.Request(url + '/api/queue', data=json.dumps(request).encode('utf-8'),
            headers={'Content-Type': 'application/json', 'X-Shorts-Token': library['token']}, method='POST')
        with urllib.request.urlopen(http_request, timeout=30) as response:
            queued = json.load(response)
        summary.append(dict(name=name, id=queued['id'], status='queued'))
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--queue', action='store_true')
    parser.add_argument('--port', type=int, default=8765)
    args = parser.parse_args()
    assert PROJECT.name == 'RTS_Shorts', PROJECT
    jobs = build_jobs()
    report = dict(jobs=save_jobs(jobs))
    (Path(__file__).with_name('LargeBatchReport.json')).write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    if args.queue:
        report['queue'] = queue_jobs(jobs, args.port)
    (Path(__file__).with_name('LargeBatchReport.json')).write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
