"""Swap the stock metal resources on Ger_2ndOreMountains for the Ore Mountains ones (Source/Docs/Environment/
OreMetalResources.md) in a throwaway full editor: same spot, same yaw. Then nav check, save, shots, quit.

Launch from PowerShell (Git Bash mangles /Game paths), one quoted argument string:
  UnrealEditor-Win64-DebugGame.exe D:/UE5Projects/RTS_Survival/RTS_Survival.uproject
    /Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains -ExecCmds="py <this file>" -unattended -nosplash
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -SCCProvider=None -abslog=<log>

Which resource goes where: the two cast ones (ingot sled, girders) on the bench by the mine railway, the two grown
ones (hematite domes, pyrite cubes) plus a second ingot sled in the west valley. Ore_dress.json is updated to match,
so step_resources() reproduces the result. A second run changes nothing (no stock metal resource left to swap).
Writes Saved/OreMountains/metal/place/place_result.json and shots next to it.
"""
import contextlib
import io
import json
import math
import os
import sys
import traceback

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap')
import ore_map_build as omb  # noqa: E402
import ore_nav_check  # noqa: E402

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/metal/place/'
UMAP = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains.umap'
DRESS = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_dress.json'
ORE = 'Metal/OreMountains/Resource_Metal_Ore_'
# feature id -> resources in the order of the feature's points
PLAN = {'metal_bench': ['Blutbarren', 'Gelbguss'], 'metal_valley_w': ['Glaskopf', 'Katzengold', 'Blutbarren']}
os.makedirs(OUT, exist_ok=True)
S = {'phase': 'load', 't': 0.0, 'mark': 0.0, 'res': {'log': []}, 'cam': None, 'views': None, 'placed': []}


def log(m):
    S['res']['log'].append(str(m))
    unreal.log_warning('ORE_METAL ' + str(m))


def dump():
    with open(OUT + 'place_result.json', 'w') as f:
        json.dump(S['res'], f, indent=1)


def quit_now():
    S['phase'] = 'done'
    dump()
    unreal.SystemLibrary.quit_editor()


def wanted():
    """[(x, y, resource name)] from the layout features."""
    out = []
    for fid, names in PLAN.items():
        for k, (x, y) in enumerate(omb.feature(fid)['points']):
            out.append((x, y, names[k % len(names)]))
    return out


def swap():
    stock = [a for a in omb.all_actors() if a.get_class().get_name().startswith('Resource_Metal_')
             and not a.get_class().get_name().startswith('Resource_Metal_Ore_')]
    already = [a for a in omb.all_actors() if a.get_class().get_name().startswith('Resource_Metal_Ore_')]
    log('stock metal resources on the map: %d, ore ones already there: %d' % (len(stock), len(already)))
    dress = json.load(open(DRESS))
    rows = dress['resources']
    report = []
    for x, y, name in wanted():
        near = sorted(stock, key=lambda a: math.hypot(a.get_actor_location().x - x, a.get_actor_location().y - y))
        if not near or math.hypot(near[0].get_actor_location().x - x, near[0].get_actor_location().y - y) > 300.0:
            log('no stock metal resource within 3 m of (%d, %d): nothing swapped there' % (x, y))
            continue
        old = near[0]
        stock.remove(old)
        loc, rot, old_label, folder = old.get_actor_location(), old.get_actor_rotation(), old.get_actor_label(), old.get_folder_path()
        cls = unreal.EditorAssetLibrary.load_blueprint_class(omb.RESOURCE_ROOT + ORE + name)
        if cls is None:
            raise RuntimeError('missing Blueprint ' + ORE + name)
        index = next((i for i, r in enumerate(rows) if r[0].startswith('Metal/') and math.hypot(r[1] - x, r[2] - y) < 300.0), None)
        omb.actors_sub().destroy_actor(old)
        actor = omb.actors_sub().spawn_actor_from_class(cls, loc, rot)
        label = '%sRes_%s_%02d' % (omb.PREFIX, name, index if index is not None else 90 + len(report))
        actor.set_actor_label(label)
        actor.set_folder_path(folder)
        if index is not None:
            rows[index][0] = ORE + name
        origin, extent = actor.get_actor_bounds(False)
        report.append(dict(label=label, was=old_label, at_cm=[round(loc.x), round(loc.y), round(loc.z)], yaw=round(rot.yaw, 1),
                           ground_z=round(omb.ground_z(loc.x, loc.y)), bounds_extent_cm=[round(extent.x), round(extent.y), round(extent.z)],
                           scale=round(actor.get_actor_scale3d().x, 3), dress_row=index))
        S['placed'].append(actor)
        log('swapped %s -> %s at (%d, %d, %d)' % (old_label, label, loc.x, loc.y, loc.z))
    if stock:
        log('stock metal resources left untouched: %s' % [a.get_actor_label() for a in stock])
    if report:
        json.dump(dress, open(DRESS, 'w'), indent=1)
    S['res']['placed'] = report
    return report


def reach_check():
    """A harvester has to get next to each resource: path from the main ramp start to a point 9.5 m off, per nav agent."""
    world = omb.world()
    navs = {a.get_actor_label().split('-')[-1]: a for a in omb.all_actors() if isinstance(a, unreal.RecastNavMesh)}
    x0, y0 = omb.feature('ramp_main')['centreline'][0]
    start = unreal.Vector(x0, y0, omb.ground_z(x0, y0) + 20.0)
    out = {}
    for actor in S['placed']:
        loc = actor.get_actor_location()
        for agent, nav in navs.items():
            best = None
            for deg in range(0, 360, 45):
                px, py = loc.x + 950.0 * math.cos(math.radians(deg)), loc.y + 950.0 * math.sin(math.radians(deg))
                goal = unreal.Vector(px, py, omb.ground_z(px, py) + 20.0)
                path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, goal, nav, None)
                if path is not None and len(path.path_points) >= 2 and not path.is_partial():
                    best = (best or 0) + 1
            out['%s %s' % (actor.get_actor_label(), agent)] = '%d of 8 sides reachable' % (best or 0)
    return out


def game_eye(target, yaw=-21.0, pitch=55.0, height=4000.0):
    d = height / math.tan(math.radians(pitch))
    return unreal.Vector(target.x - math.cos(math.radians(yaw)) * d, target.y - math.sin(math.radians(yaw)) * d, target.z + height)


def views():
    out = []
    groups = {}
    for actor in S['placed']:
        loc = actor.get_actor_location()
        groups.setdefault('bench' if loc.x < -12000 else 'valley', []).append(loc)
        out.append((actor.get_actor_label(), game_eye(loc, height=1500.0), loc))
    for name, locs in groups.items():
        mid = unreal.Vector(sum(p.x for p in locs) / len(locs), sum(p.y for p in locs) / len(locs), sum(p.z for p in locs) / len(locs))
        out.insert(0, ('group_' + name, game_eye(mid, height=4500.0), mid))
    return out


def tick(dt):
    S['t'] += dt
    try:
        ph = S['phase']
        if ph == 'load' and S['t'] > 20.0:
            S['phase'] = 'placing'
            omb.require_map()
            S['res']['umap_before'] = os.path.getsize(UMAP)
            omb.load_layout()
            if not swap():
                log('nothing to swap: map not saved')
                quit_now()
                return
            unreal.SystemLibrary.execute_console_command(omb.world(), 'RebuildNavigation')
            S['phase'], S['mark'] = 'nav', S['t']
            log('placed; navigation rebuild started')
        elif ph == 'nav' and S['t'] - S['mark'] > 8.0 and omb.nav_ready():
            S['phase'] = 'saving'          # the save ticks Slate again: never re-enter this branch
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                ore_nav_check.run()
            S['res']['nav_check'] = buf.getvalue().splitlines()
            S['res']['reach'] = reach_check()
            log('nav check done')
            S['res']['saved'] = bool(omb.save_level())
            S['phase'], S['mark'] = 'saved', S['t']
        elif ph == 'saved' and S['t'] - S['mark'] > 3.0:
            S['res']['umap_after'] = os.path.getsize(UMAP)
            if S['res']['umap_after'] < 0.9 * S['res']['umap_before']:
                log('WARNING umap shrank: %d -> %d' % (S['res']['umap_before'], S['res']['umap_after']))
            unreal.SystemLibrary.execute_console_command(omb.world(), 'ShowFlag.PostProcessMaterial 0')
            S['cam'] = omb.actors_sub().spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 5000))
            S['cam'].camera_component.set_editor_property('field_of_view', 70.0)
            S['views'] = views()
            S['phase'], S['mark'] = 'shots', S['t']
        elif ph == 'shots' and S['t'] - S['mark'] > 4.0:
            if not S['views']:
                omb.actors_sub().destroy_actor(S['cam'])      # never saved: the map was saved before the camera
                S['phase'], S['mark'] = 'quit', S['t']
                return
            name, eye, at = S['views'].pop(0)
            S['cam'].set_actor_location(eye, False, True)
            S['cam'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, at), True)
            S['phase'], S['mark'] = 'shots', S['t']
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, OUT + name + '.png', camera=S['cam'])
            log('shot ' + name)
        elif ph == 'quit' and S['t'] - S['mark'] > 8.0:
            quit_now()
    except Exception:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log('ERROR ' + traceback.format_exc())
        quit_now()


unreal.register_slate_post_tick_callback(tick)
