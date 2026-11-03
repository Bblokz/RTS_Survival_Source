"""Check the three-battle batch against native spawns, real drops, captions and exported media."""
from collections import Counter
from fractions import Fraction
import json
from pathlib import Path
import re
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[3]
SHORTS = PROJECT / 'ShortsCreator'


def class_name(path):
    return path.split('.')[-1]


def spawned_class(actor):
    return actor.rsplit('_', 1)[0]


def audit(take, expected):
    job = json.loads((take / 'Job.json').read_text(encoding='utf-8-sig'))
    manifest = json.loads((take / 'Manifest.json').read_text(encoding='utf-8-sig'))
    log = (take / 'render.log').read_text(encoding='utf-8', errors='replace')
    segment = job['Segments'][0]
    name = job['JobName']
    video = take / (name + '.mp4')
    probe = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_entries',
        'format=duration:stream=codec_type,width,height,avg_frame_rate,nb_frames,channels',
        '-of', 'json', str(video)], text=True))
    picture = next(stream for stream in probe['streams'] if stream['codec_type'] == 'video')
    assert (picture['width'], picture['height'], Fraction(picture['avg_frame_rate'])) == (1080, 1920, 60)
    assert any(stream['codec_type'] == 'audio' and stream['channels'] == 2 for stream in probe['streams'])
    duration = float(probe['format']['duration'])
    assert 2 < duration <= 120.001
    assert abs(int(picture['nb_frames']) / 60 - duration) < .02
    assert manifest['bComplete'] and 'RTS_SHORTS_JOB_DONE job=' + name + ' success=true' in log
    assert 'RTS_SHORTS_PARATROOPER_CUT local=120 introFrames=120' in log
    own = [segment['ClassPath']] + segment['BattleAllyClassPaths']
    enemy = segment['BattleEnemyClassPaths']
    wanted = Counter((class_name(path), '2') for path in own[1:])
    wanted.update((class_name(path), '1') for path in enemy)
    actual = Counter((spawned_class(actor), owner) for actor, owner in re.findall(
        r'RTS_SHORTS_BATTLE_PARTICIPANT unit=(\S+) owner=(\d+)', log))
    assert actual == wanted, (actual - wanted, wanted - actual)
    assert 'RTS_SHORTS_SPAWNED index=0 unit=' + class_name(own[0]) + '_' in log
    drops = re.findall(r'RTS_SHORTS_PARATROOPER_SQUAD squad=(\S+) owner=(\d+)', log)
    wanted_drops = Counter({(class_name(path), str(expected['drop_owner'])): count
                            for path, count in expected['expected_drops'].items()})
    assert Counter((spawned_class(actor), owner) for actor, owner in drops) == wanted_drops, drops
    classes = set(own + enemy + list(expected['expected_drops']))
    assert len(classes) == 6
    assert segment['Caption1'] == expected['captions'][0] and segment['Caption2'] == expected['captions'][1]
    assert len(segment['Caption1'].split('|')) == len(segment['Caption2'].split('|')) == 4
    passes = re.findall(r'RTS_SHORTS_PARATROOPER_BEGIN [^\r\n]*pass=(\d+) owner=(\d+)', log)
    assert passes == [('0', str(expected['drop_owner'])), ('1', str(expected['drop_owner']))], passes
    assert 'RTS_SHORTS_PARATROOPER_AIRCRAFT count=2' in log
    jumps = re.findall(r'RTS_PARATROOPER_JUMP run=(\d+) index=(\d+) unit=(\S+) source=(\w+)', log)
    assert jumps and {run for run, index, actor, source in jumps} == {'0', '1'}
    assert all(source == 'aircraft' for run, index, actor, source in jumps)
    owners = re.findall(r'RTS_SHORTS_OWNER_READY squad=(\S+) soldier=(\S+) owner=(\d+) expected=(\d+)', log)
    assert owners and all(owner == expected_owner for squad, actor, owner, expected_owner in owners)
    ranges = set(re.findall(r'RTS_SHORTS_DOUBLE_RANGE unit=(\S+)', log))
    expected_classes = Counter(class_name(path) for path in own + enemy)
    expected_classes.update({class_name(path): count for path, count in expected['expected_drops'].items()})
    assert Counter(spawned_class(actor) for actor in ranges) == expected_classes
    report = dict(name=name, video=str(video), duration_seconds=duration,
        resolution=[1080, 1920], fps=60, stereo_audio=True, combat_types=6,
        actual_ground_entities=len(own) + len(enemy), dropped_squads=len(drops),
        drop_soldiers=len(jumps), drop_owner=expected['drop_owner'],
        doubled_range_recipients=len(ranges), soldier_owner_checks=len(owners),
        aircraft=2, intro_seconds=2, success=True)
    (take / 'Verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


def main():
    plans = json.loads(Path(__file__).with_name('LargeBatchReport.json').read_text(encoding='utf-8'))['jobs']
    reports = []
    for expected in plans:
        candidates = sorted((SHORTS / 'Output' / expected['name']).glob('take_*'))
        finished = [take for take in candidates if (take / (expected['name'] + '.mp4')).is_file()]
        if not finished:
            print(expected['name'] + ': pending')
            continue
        reports.append(audit(finished[-1], expected))
    print(json.dumps(reports, indent=2))
    if len(reports) == 3:
        Path(__file__).with_name('LargeBatchVerification.json').write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
