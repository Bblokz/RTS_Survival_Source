"""Post-process one POI preview run: map overlay, contact sheets and the layout-JSON exclusion check.

usage: python poi_post.py <run dir> <seed> [layout json]
Reads poi_<seed>.json and cameras_poi_<seed>.json written by -SurvivalPOIPreview (SurvivalPOIPreview.cpp).
"""
import json
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

TYPE_COLOURS = {
    'battlefield': (230, 60, 50), 'hangar': (60, 160, 255), 'crashsite': (255, 200, 40),
    'convoy': (255, 120, 220), 'graveyard': (190, 90, 255), 'pumpstation': (60, 230, 200), 'relaystation': (240, 240, 240),
}
RAMP_HALF_WIDTH = 1100.0
GATE_FOOT_CLEARANCE = 3000.0
# Rubric B.3: at least 2 500 cm of free drivable width where a POI meets a route.
ROUTE_FREE_WIDTH = 2500.0


def load(path):
    with open(path, encoding='utf-8') as handle:
        return json.load(handle)


def font(size):
    for name in ('arialbd.ttf', 'arial.ttf', 'DejaVuSans-Bold.ttf'):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


class TopProjector:
    """World XY to pixels for a straight-down shot (pitch -90, yaw 0: screen up = +X, screen right = +Y)."""

    def __init__(self, shot, width, height):
        self.eye = shot['eye']
        self.width = width
        self.height = height
        self.focal = (width / 2.0) / math.tan(math.radians(shot['fov_horizontal'] / 2.0))

    def __call__(self, x, y, z=800.0):
        depth = max(1.0, self.eye[2] - z)
        return (self.width / 2.0 + (y - self.eye[1]) * self.focal / depth,
                self.height / 2.0 - (x - self.eye[0]) * self.focal / depth)

    def scale(self, z=800.0):
        return self.focal / max(1.0, self.eye[2] - z)


def draw_map(run_dir, seed, manifest, cameras):
    shot = next((s for s in cameras['shots'] if s['file'] == f'map_pois_{seed}_clean.png'), None)
    clean = os.path.join(run_dir, f'map_pois_{seed}_clean.png')
    if shot is None or not os.path.exists(clean):
        print('no clean map shot')
        return
    image = Image.open(clean).convert('RGB')
    draw = ImageDraw.Draw(image)
    project = TopProjector(shot, image.width, image.height)
    scale = project.scale()
    small, big = font(14), font(18)
    run = manifest['run']
    centre = run['basin_centre']
    # Sector lines: from the basin centre out through every start.
    for start in run['starts']:
        sx, sy, sz = start['centre']
        angle = math.atan2(sy - centre[1], sx - centre[0])
        far = (centre[0] + math.cos(angle) * 90000, centre[1] + math.sin(angle) * 90000)
        draw.line([project(centre[0], centre[1]), project(*far)], fill=(255, 255, 255), width=2)
        px, py = project(sx, sy, sz)
        radius = 18000 * scale
        draw.ellipse([px - radius, py - radius, px + radius, py + radius], outline=(255, 255, 0), width=2)
        draw.text((px + 6, py - 20), f"S{start['index']} {start['name']}", fill=(255, 255, 0), font=big)
    for sector in run['sectors']:
        mid = math.radians(0.5 * (sector['angle_a_deg'] + sector['angle_b_deg']))
        label = (centre[0] + math.cos(mid) * 52000, centre[1] + math.sin(mid) * 52000)
        built = '' if sector['landmark_built'] else ' (not built)'
        draw.text(project(*label), f"sector {sector['sector']}: {sector['landmark']}{built}", fill=(255, 255, 255), font=small)
    oasis = (16000.0, -3525.0, 6150.0, 3000.0)
    ox, oy = project(oasis[0], oasis[1])
    for radius, colour in ((oasis[2], (0, 220, 255)), (oasis[2] + oasis[3], (0, 150, 200))):
        r = radius * scale
        draw.ellipse([ox - r, oy - r, ox + r, oy + r], outline=colour, width=2)
    draw.text((ox + 6, oy + 6), 'oasis basin / +3000 rim', fill=(0, 220, 255), font=small)
    for poi in manifest['pois']:
        x, y, z = poi['centre']
        px, py = project(x, y, z)
        r = poi['radius'] * scale
        colour = TYPE_COLOURS.get(poi['type'], (255, 255, 255))
        draw.ellipse([px - r, py - r, px + r, py + r], outline=colour, width=3)
        tag = ' LANDMARK' if poi['landmark'] else ''
        draw.text((px + r + 3, py - 9), f"{poi['type']} #{poi['id']}{tag}", fill=colour, font=big)
        for actor in poi['actors']:
            if actor.get('alive') and actor['role'] in ('scavenge', 'capture'):
                ax, ay, az = actor['xyz']
                qx, qy = project(ax, ay, az)
                fill = (80, 255, 80) if actor['role'] == 'scavenge' else (80, 200, 255)
                draw.rectangle([qx - 2, qy - 2, qx + 2, qy + 2], fill=fill)
    for group in manifest['wildlife']:
        x, y, z = group['spawn_centre']
        px, py = project(x, y, z)
        colour = (255, 255, 255) if group['kind'] == 'ground group' else (200, 200, 255)
        draw.polygon([(px, py - 7), (px - 6, py + 5), (px + 6, py + 5)], outline=colour)
        draw.text((px + 7, py), f"{group['name']} x{group['count']}", fill=colour, font=small)
    # Straight lines from each start to its nearest scavenge and capture target (chained nav length in the label).
    for row in manifest['fairness']['per_start']:
        start = next(s for s in run['starts'] if s['index'] == row['start'])
        for kind, colour in (('scavenge', (80, 255, 80)), ('capture', (80, 200, 255))):
            target = nearest_target(manifest, start['centre'], kind)
            if target is None:
                continue
            a = project(*start['centre'])
            b = project(*target)
            draw.line([a, b], fill=colour, width=1)
            nav = row.get(f'{kind}_nav_heavytank', -1)
            mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
            draw.text(mid, f"{kind} {nav / 100:.0f} m" if nav > 0 else f'{kind} no path', fill=colour, font=small)
    draw.text((10, 10), f"seed {seed}  picked start {run['picked_start']}  {len(manifest['pois'])} POIs  "
                        f"{len(manifest['wildlife'])} wildlife groups  (straight lines; label = chained HeavyTank nav length)",
              fill=(255, 255, 255), font=big)
    image.save(os.path.join(run_dir, f'map_pois_{seed}.png'))


def nearest_target(manifest, centre, role):
    best, best_distance = None, 1e18
    for poi in manifest['pois']:
        for actor in poi['actors']:
            if actor.get('alive') and actor['role'] == role:
                distance = math.dist(actor['xyz'][:2], centre[:2])
                if distance < best_distance:
                    best, best_distance = actor['xyz'], distance
    return best


def contact_sheet(run_dir, seed, suffix, name, columns=3, cell=(640, 360)):
    files = sorted(f for f in os.listdir(run_dir) if f.endswith(suffix) and f'_{seed}' in f
                   and not f.startswith(('contact_', 'map_pois_', 'strip_')))
    if not files:
        return
    rows = (len(files) + columns - 1) // columns
    sheet = Image.new('RGB', (columns * cell[0], rows * (cell[1] + 24)), (20, 20, 20))
    draw = ImageDraw.Draw(sheet)
    label_font = font(16)
    for index, file_name in enumerate(files):
        image = Image.open(os.path.join(run_dir, file_name)).convert('RGB')
        image.thumbnail(cell)
        x = (index % columns) * cell[0]
        y = (index // columns) * (cell[1] + 24)
        sheet.paste(image, (x, y + 24))
        draw.text((x + 4, y + 3), file_name, fill=(255, 255, 255), font=label_font)
    sheet.save(os.path.join(run_dir, name), quality=88)


def flock_strips(run_dir, seed, cell=(480, 270)):
    groups = {}
    for name in os.listdir(run_dir):
        if name.startswith('strip_') and f'_{seed}_' in name and name.endswith('.png'):
            groups.setdefault(name.rsplit('_f', 1)[0], []).append(name)
    for stem, files in groups.items():
        files.sort()
        sheet = Image.new('RGB', (5 * cell[0], 2 * (cell[1] + 22)), (20, 20, 20))
        draw = ImageDraw.Draw(sheet)
        for index, file_name in enumerate(files[:10]):
            image = Image.open(os.path.join(run_dir, file_name)).convert('RGB')
            image.thumbnail(cell)
            x = (index % 5) * cell[0]
            y = (index // 5) * (cell[1] + 22)
            sheet.paste(image, (x, y + 22))
            draw.text((x + 4, y + 2), f't+{index} s', fill=(255, 255, 255), font=font(16))
        out = stem.replace('strip_', 'flock_') + '_strip.jpg'
        sheet.save(os.path.join(run_dir, out), quality=88)


def point_in_polygon(x, y, polygon):
    inside = False
    j = len(polygon) - 1
    for i in range(len(polygon)):
        xi, yi = polygon[i]
        xj, yj = polygon[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / ((yj - yi) or 1e-9) + xi:
            inside = not inside
        j = i
    return inside


def distance_to_polyline(x, y, line):
    best = 1e18
    for (ax, ay), (bx, by) in zip(line, line[1:]):
        dx, dy = bx - ax, by - ay
        length = dx * dx + dy * dy
        t = 0.0 if length == 0 else max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / length))
        best = min(best, math.hypot(x - (ax + t * dx), y - (ay + t * dy)))
    return best


def check_layout(run_dir, seed, manifest, layout_path):
    layout = load(layout_path)
    starts = layout['starts']
    lines = []
    violations = []
    checked = 0
    for poi in manifest['pois']:
        for actor in poi['actors']:
            if not actor.get('alive'):
                continue
            checked += 1
            x, y, _ = actor['xyz']
            box_min = actor.get('lod0_box_local_min', [0, 0, 0])
            box_max = actor.get('lod0_box_local_max', [0, 0, 0])
            reach = 0.5 * math.hypot(box_max[0] - box_min[0], box_max[1] - box_min[1])
            for landform in layout['features']['landforms']:
                start = starts[landform['start']]
                for ramp in landform['ramps']:
                    corridor = distance_to_polyline(x, y, ramp['line'])
                    if corridor < RAMP_HALF_WIDTH + reach:
                        violations.append(f"#{poi['id']} {poi['type']} {actor['asset']}: in ramp corridor {landform['name']}/{ramp['name']} ({corridor:.0f})")
                    foot = max(ramp['line'], key=lambda p: math.hypot(p[0] - start['x'], p[1] - start['y']))
                    if math.hypot(x - foot[0], y - foot[1]) < GATE_FOOT_CLEARANCE + reach:
                        violations.append(f"#{poi['id']} {poi['type']} {actor['asset']}: within 3000 of gate foot {landform['name']}/{ramp['name']}")
                for pad in landform['pads']:
                    if point_in_polygon(x, y, pad['outline']):
                        violations.append(f"#{poi['id']} {poi['type']} {actor['asset']}: on pad {landform['name']}/{pad['name']}")
            for world in layout['features']['world']:
                for pad in world.get('pads', []):
                    if point_in_polygon(x, y, pad['outline']):
                        violations.append(f"#{poi['id']} {poi['type']} {actor['asset']}: on pad {world['name']}/{pad['name']}")
                for basin in world.get('basins', []):
                    if point_in_polygon(x, y, basin['outline']):
                        violations.append(f"#{poi['id']} {poi['type']} {actor['asset']}: inside basin {basin['name']}")
    # Routes (layout `routes`): where a POI touches one, the drivable width left beside the route line. A route is taken as a
    # corridor of ROUTE_FREE_WIDTH centred on its line; any actor box reaching into it narrows the route.
    route_lines = []
    for name, line in (layout.get('routes') or {}).items():
        nearest = None
        for poi in manifest['pois']:
            for actor in poi['actors']:
                if not actor.get('alive'):
                    continue
                x, y, _ = actor['xyz']
                box_min = actor.get('lod0_box_local_min', [0, 0, 0])
                box_max = actor.get('lod0_box_local_max', [0, 0, 0])
                reach = 0.5 * math.hypot(box_max[0] - box_min[0], box_max[1] - box_min[1])
                edge = distance_to_polyline(x, y, line) - reach
                if nearest is None or edge < nearest[0]:
                    nearest = (edge, poi['id'], poi['type'], actor['asset'])
        if nearest is None:
            continue
        clear = nearest[0] >= ROUTE_FREE_WIDTH * 0.5
        route_lines.append(f"route {name}: nearest POI actor #{nearest[1]} {nearest[2]} {nearest[3]} edge {nearest[0]:.0f} cm from the line"
                           f" -> {'clear' if clear else 'NARROWS THE ROUTE'}")
        if not clear:
            violations.append(f"#{nearest[1]} {nearest[2]} {nearest[3]}: within {ROUTE_FREE_WIDTH * 0.5:.0f} of route {name} ({nearest[0]:.0f})")
    lines.append(f'layout JSON {layout_path}')
    lines.append(f'actors checked against ramps (corridor +-{RAMP_HALF_WIDTH:.0f} + half the LOD0 box diagonal), gate feet ({GATE_FOOT_CLEARANCE:.0f}), landform pads and the oasis basin outline: {checked}')
    lines.append(f'violations: {len(violations)}')
    lines.extend(violations)
    lines.append(f'routes ({len(route_lines)}; a POI actor must keep {ROUTE_FREE_WIDTH * 0.5:.0f} cm off the line, so {ROUTE_FREE_WIDTH:.0f} cm stay free):')
    lines.extend(route_lines)
    with open(os.path.join(run_dir, f'layout_check_{seed}.txt'), 'w', encoding='utf-8') as handle:
        handle.write('\n'.join(lines) + '\n')
    print('\n'.join(lines[:12]))


def main():
    run_dir, seed = sys.argv[1], sys.argv[2]
    layout = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(__file__), '..', '..', 'Docs', 'LevelTools', 'SM_Desert_01_layout.json')
    manifest = load(os.path.join(run_dir, f'poi_{seed}.json'))
    cameras = load(os.path.join(run_dir, f'cameras_poi_{seed}.json'))
    draw_map(run_dir, seed, manifest, cameras)
    contact_sheet(run_dir, seed, '_top.png', f'contact_top_{seed}.jpg')
    contact_sheet(run_dir, seed, '_rts.png', f'contact_rts_{seed}.jpg')
    contact_sheet(run_dir, seed, '_rts5200.png', f'contact_rts5200_{seed}.jpg')
    files = [f for f in os.listdir(run_dir) if '_graze_' in f and f'_{seed}' in f]
    if files:
        contact_sheet(run_dir, seed, tuple(sorted({f[f.index('_graze_'):] for f in files})), f'contact_graze_{seed}.jpg', columns=2)
    contact_sheet(run_dir, seed, tuple(f'_t{t}.png' for t in (0, 5, 10)) + ('_close.png', '_flee_t0.png', '_flee_t3.png', '_flee_t8.png'),
                  f'contact_wild_{seed}.jpg')
    flock_strips(run_dir, seed)
    check_layout(run_dir, seed, manifest, layout)


if __name__ == '__main__':
    main()
