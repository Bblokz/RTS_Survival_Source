"""Copy the map layout's ramps, pads and basins into DA_SurvivalPointsOfInterest (Placement.Layout).

Run inside the Unreal editor's Python (headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>), after
the terrain changed and the layout JSON was refreshed. Re-runnable: it replaces the whole Layout block. The runtime warns
(LogSurvivalPOI) when the stored MD5 no longer matches the layout file.
"""
import hashlib
import json
import math
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LAYOUT_RELATIVE = 'Source/RTS_Survival/SurvivalMode/Docs/LevelTools/SM_Desert_01_layout.json'
ASSET_PATH = '/Game/RTS_Survival/SurvivalMode/PointsOfInterest/DA_SurvivalPointsOfInterest'
# Pads a POI may stand on: the salt pan is the crash site's natural ground.
PAD_NAMES_ALLOWED = ('salt',)
BASIN_CLEARANCE = 1750.0
# Power line pylons: points of interest keep this far (plus their own radius) from every pole.
POLE_CLEARANCE = 1500.0
# Routes are copied as lines (Layout.Routes): sites keep (radius + RouteSiteClearance, -900) off them, standing objects keep
# RouteHalfWidth (1 250, rubric B.3: 2 500 free) plus their footprint off them, and the dead convoy may lie along one.


def vec2(point):
    return unreal.Vector2D(float(point[0]), float(point[1]))


def make_line(name, points, foot):
    line = unreal.SurvivalPOILayoutLine()
    line.set_editor_property('name', name)
    line.set_editor_property('points', [vec2(p) for p in points])
    line.set_editor_property('foot_point', vec2(foot))
    return line


def make_area(name, outline, clearance):
    area = unreal.SurvivalPOILayoutArea()
    area.set_editor_property('name', name)
    area.set_editor_property('outline', [vec2(p) for p in outline])
    area.set_editor_property('clearance', float(clearance))
    return area


def sync(asset=None, layout_relative=LAYOUT_RELATIVE):
    path = os.path.join(PROJECT, layout_relative)
    raw = open(path, 'rb').read()
    layout = json.loads(raw.decode('utf-8'))
    md5 = hashlib.md5(raw).hexdigest()
    starts = layout['starts']
    ramps, areas = [], []
    for landform in layout['features']['landforms']:
        start = starts[landform['start']]
        for ramp in landform.get('ramps', []):
            points = ramp['line']
            foot = max(points, key=lambda p: math.hypot(p[0] - start['x'], p[1] - start['y']))
            ramps.append(make_line(f"{landform['name']}/{ramp['name']}", points, foot))
        for pad in landform.get('pads', []):
            if any(word in pad['name'] for word in PAD_NAMES_ALLOWED):
                continue
            areas.append(make_area(f"{landform['name']}/{pad['name']}", pad['outline'], 0.0))
    for world in layout['features']['world']:
        for pad in world.get('pads', []):
            if any(word in pad['name'] for word in PAD_NAMES_ALLOWED):
                continue
            areas.append(make_area(f"{world['name']}/{pad['name']}", pad['outline'], 0.0))
        for basin in world.get('basins', []):
            # Round 9 resized the oasis to 75%: its own top-level block holds the current outline.
            outline = layout['oasis']['outline'] if basin['name'] == 'oasis' and 'oasis' in layout else basin['outline']
            areas.append(make_area(f"{world['name']}/{basin['name']}", outline, BASIN_CLEARANCE))
    obstacles = []
    routes = [make_line(name, line, line[-1]) for name, line in (layout.get('routes') or {}).items() if len(line) >= 2]
    for line in layout.get('power_lines', []):
        for pole in line.get('poles', []):
            obstacles.append(unreal.Vector(float(pole['x']), float(pole['y']), POLE_CLEARANCE))
    if asset is None:
        asset = unreal.load_asset(ASSET_PATH)
    placement = asset.get_editor_property('placement')
    exclusions = placement.get_editor_property('layout')
    exclusions.set_editor_property('layout_file', layout_relative)
    exclusions.set_editor_property('layout_md5', md5)
    exclusions.set_editor_property('ramps', ramps)
    exclusions.set_editor_property('areas', areas)
    exclusions.set_editor_property('obstacles', obstacles)
    exclusions.set_editor_property('routes', routes)
    placement.set_editor_property('layout', exclusions)
    asset.set_editor_property('placement', placement)
    print(f'POI_LAYOUT synced {len(ramps)} ramps, {len(areas)} areas, {len(obstacles)} obstacles, {len(routes)} routes, md5 {md5}')
    return asset


if __name__ == '__main__':
    synced = sync()
    print('POI_LAYOUT saved', unreal.EditorAssetLibrary.save_loaded_asset(synced, only_if_is_dirty=False))
