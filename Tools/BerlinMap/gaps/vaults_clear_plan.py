"""Brewery vaults next to the hall on Ger_Berlin: which instances / actors stand in the vaults' footprint.
Offline, from gaps_dump.py's components.json. Writes Saved/Berlin/gaps/vaults_plan.json."""
import json
import math

import numpy as np

import gaps_scan as gs

d = json.load(open(gs.OUT + 'components.json'))
hall = [r for r in d['rows'] if r['a'] == 'BP_Berlin_BreweryHall' and d['meshes'][r['m']].endswith('SM_Berlin_BreweryHall')][0]['xf']
yaw = math.degrees(math.atan2(2 * (hall[6] * hall[5] + hall[3] * hall[4]), 1 - 2 * (hall[4] ** 2 + hall[5] ** 2)))
ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
HALL_MIN_X, V_MIN, V_MAX, GAP = -1444.0, (-1711.0, -1620.0), (1620.0, 1842.0), 150.0
off = HALL_MIN_X - V_MAX[0] - GAP                       # vaults pivot in the hall's frame: on its -X side
vx, vy = hall[0] + off * ca, hall[1] + off * sa
MARGIN = 120.0


def inside(x, y, m=MARGIN):
    lx, ly = (x - vx) * ca + (y - vy) * sa, -(x - vx) * sa + (y - vy) * ca
    return (lx > V_MIN[0] - m) & (lx < V_MAX[0] + m) & (ly > V_MIN[1] - m) & (ly < V_MAX[1] + m)


Z = gs.load_heights('final_height.r16')
xs, ys = np.meshgrid(np.linspace(V_MIN[0], V_MAX[0], 30), np.linspace(V_MIN[1], V_MAX[1], 30))
wx, wy = vx + xs * ca - ys * sa, vy + xs * sa + ys * ca
ground = gs.ground_at(Z, wx, wy)
vz = float(np.percentile(ground, 60))
print('hall at %.0f %.0f yaw %.1f; vaults at %.0f %.0f z %.1f (ground under them %.0f..%.0f)' % (
    hall[0], hall[1], yaw, vx, vy, vz, ground.min(), ground.max()))
scan = gs.Scan()
inst, actors = [], {}
for item in scan.items:
    p = scan.pts(item['m']) * np.array(item['xf'][7:10])
    p = p @ gs.rot_matrix(item['xf'][3:7]).T + np.array(item['xf'][0:3])
    hit = inside(p[:, 0], p[:, 1])
    low = hit & (p[:, 2] < vz + 600.0)
    if not low.any():
        continue
    share = float(hit.mean())
    name = d['meshes'][item['m']].split('.')[-1]
    if item['a'] == 'BP_Berlin_BreweryHall':
        continue
    if item['k'] >= 0:
        inst.append({'a': item['a'], 'c': item['c'], 'k': item['k'], 'xy': item['xf'][:2], 'mesh': name, 'share': round(share, 2)})
    else:
        actors[item['a']] = {'a': item['a'], 'xy': item['xf'][:2], 'mesh': name, 'share': round(share, 2), 'cls': item['cls']}
for e in inst:
    print('instance', e['a'], e['k'], e['mesh'], e['share'])
for e in actors.values():
    print('actor   ', e['a'], e['cls'], e['mesh'], e['share'])
json.dump({'vaults': [vx, vy, vz, yaw], 'instances': inst, 'actors': list(actors.values())}, open(gs.OUT + 'vaults_plan.json', 'w'), indent=1)
