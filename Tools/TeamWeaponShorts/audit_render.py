"""Accept a team-weapons take only after its towing, deployment and media checks pass."""
import argparse
import json
from pathlib import Path
import re
import subprocess


def audit(take):
    manifest = json.loads((take / 'Manifest.json').read_text())
    job = json.loads((take / 'Job.json').read_text())
    log = (take / 'render.log').read_text(errors='replace')
    assert manifest['bComplete'], 'Capture did not complete'
    assert len(manifest['segments']) == 2
    intro, battle = manifest['segments']
    fps = manifest['fps']
    pair_count = len(job['Segments'][0]['TeamWeapons']['SquadClassPaths'])
    assert not any(shot['bSkipped'] for shot in manifest['segments'])
    assert intro['endFrame'] == battle['startFrame']
    assert intro['endFrame'] - intro['startFrame'] == 2 * fps
    assert manifest['totalFrames'] - manifest['warmupFrames'] <= 120 * fps
    towed = re.findall(r'TEAM_WEAPON_TOWED frame=(\d+) vehicle=(\S+) weapon=(\S+) crew=(\S+)', log)
    assert len(towed) == pair_count, 'Every transport must hook its own gun'
    assert all(int(frame) < intro['startFrame'] for frame, *_ in towed)
    motions = re.findall(r'TEAM_WEAPON_MOTION frame=(\d+) vehicle=(\S+) attached=(\d) speed=([\d.]+) distance=([\d.]+)', log)
    motion_by_vehicle = {}
    for frame, vehicle, attached, speed, distance in motions:
        if intro['startFrame'] <= int(frame) < intro['endFrame']:
            assert attached == '1', 'Gun detached during the visible intro'
            motion_by_vehicle.setdefault(vehicle, []).append((float(speed), float(distance)))
    assert len(motion_by_vehicle) == pair_count, 'Need visible-shot motion evidence for every transport'
    assert all(max(speed for speed, _ in records) > 50 for records in motion_by_vehicle.values()), 'A transport remained stationary'
    deployed = re.findall(r'TEAM_WEAPON_DEPLOYED frame=(\d+) weapon=(\S+) crew=(\S+)', log)
    assert len(deployed) == pair_count, 'All crews must finish native deployment'
    assert all(int(frame) >= battle['startFrame'] for frame, *_ in deployed)
    participants = re.findall(r'BATTLE_PARTICIPANT unit=(\S+) owner=(\d+)', log)
    enemies = [name for name, owner in participants if owner == '2']
    assert sum(name.startswith('Rus_Vehicle_T3476_C_') for name in enemies) == 3
    assert sum(name.startswith('Rus_Vehicle_SU76_C_') for name in enemies) == 1
    assert 'RTS_SHORTS_JOB_DONE job=' + manifest['jobName'] + ' success=true' in log
    video = take / (manifest['jobName'] + '.mp4')
    result = subprocess.run(['ffprobe', '-v', 'error', '-show_streams', '-show_format', '-of', 'json', str(video)], capture_output=True, text=True, check=True, timeout=30)
    media = json.loads(result.stdout)
    picture = next(stream for stream in media['streams'] if stream['codec_type'] == 'video')
    sound = next(stream for stream in media['streams'] if stream['codec_type'] == 'audio')
    assert (picture['width'], picture['height']) == (job['ResolutionX'], job['ResolutionY']) == (1080, 1920)
    assert picture['r_frame_rate'] == '60/1'
    assert sound['channels'] == 2
    duration = float(media['format']['duration'])
    assert duration <= 120.05
    assert abs(duration - (battle['endFrame'] - intro['startFrame']) / fps) < .1
    report = dict(video=str(video), duration_seconds=duration, resolution='1080x1920', fps=60, audio='stereo',
                  intro_seconds=2, native_tows=len(towed), native_deployments=len(deployed),
                  visible_convoy_motion=motion_by_vehicle, soviet_vehicles=enemies)
    (take / 'TeamWeaponAudit.json').write_text(json.dumps(report, indent=2))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('take', type=Path)
    args = parser.parse_args()
    print(json.dumps(audit(args.take), indent=2))
