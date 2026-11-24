"""Place the adit portals on Ger_2ndOreMountains in a throwaway full editor, check nav, save, take shots, quit.

Launch from PowerShell (Git Bash mangles /Game paths), one quoted argument string:
  UnrealEditor-Win64-DebugGame.exe D:/UE5Projects/RTS_Survival/RTS_Survival.uproject
    /Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains -ExecCmds="py <this file>" -unattended -nosplash
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -SCCProvider=None -abslog=<log>

Writes Saved/OreMountains/adits/place_result.json and shots to Saved/OreMountains/adits/map_shots/.
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

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/adits/'
SHOTS = OUT + 'map_shots/'
UMAP = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains.umap'
os.makedirs(SHOTS, exist_ok=True)
S = {'phase': 'load', 't': 0.0, 'mark': 0.0, 'res': {'log': []}, 'cam': None, 'views': None}


def log(m):
    S['res']['log'].append(str(m))
    unreal.log_warning('ORE_ADITS ' + str(m))


def dump():
    with open(OUT + 'place_result.json', 'w') as f:
        json.dump(S['res'], f, indent=1)


def quit_now():
    S['phase'] = 'done'
    dump()
    unreal.SystemLibrary.quit_editor()


def lorry_routes(drop):
    """Path from both ends of the main ramp to the drop-off, per nav agent."""
    world = omb.world()
    navs = {a.get_actor_label().split('-')[-1]: a for a in omb.all_actors() if isinstance(a, unreal.RecastNavMesh)}
    line = omb.feature('ramp_main')['centreline']
    out = {}
    for end_name, (x, y) in (('ramp_main_start', line[0]), ('ramp_main_end', line[-1])):
        start = unreal.Vector(x, y, omb.ground_z(x, y) + 20.0)
        for agent, nav in navs.items():
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, drop, nav, None)
            ok = path is not None and len(path.path_points) >= 2 and not path.is_partial()
            last = path.path_points[-1] if path is not None and path.path_points else None
            out['%s %s' % (end_name, agent)] = dict(
                ok=ok, length_m=round(path.get_path_length() / 100.0) if path is not None else None,
                end_miss_m=round((last - drop).length() / 100.0, 1) if last is not None else None)
    return out


def game_eye(target, yaw=-21.0, pitch=55.0, height=4000.0):
    d = height / math.tan(math.radians(pitch))
    return unreal.Vector(target.x - math.cos(math.radians(yaw)) * d, target.y - math.sin(math.radians(yaw)) * d, target.z + height)


def views():
    f = omb.adit_frames()
    f1, f2 = f['adit_1'], f['adit_2']
    apron = omb.adit_world(f2, 0.0, 12.0)
    apron.z = omb.ground_z(apron.x, apron.y)
    mid = omb.adit_world(f2, -22.0, 10.0)
    mid.z = omb.ground_z(mid.x, mid.y)
    return [
        ('game', game_eye(apron), apron),
        ('game_wide', game_eye(mid, height=5500.0), mid),
        ('front', omb.adit_world(f2, 15.0, 40.0, 9.0), omb.adit_world(f2, 0.0, 2.0, 5.5)),
        ('mouth', omb.adit_world(f2, 13.0, 30.0, 4.5), omb.adit_world(f2, 0.0, 13.0, 3.6)),
        ('drift', omb.adit_world(f1, 2.0, 24.0, 5.0), omb.adit_world(f1, -5.0, 9.0, 1.5)),
        ('pair', omb.adit_world(f2, -22.0, 60.0, 18.0), omb.adit_world(f2, -22.0, 4.0, 5.0)),
    ]


def tick(dt):
    S['t'] += dt
    try:
        ph = S['phase']
        if ph == 'load' and S['t'] > 20.0:
            S['phase'] = 'placing'
            omb.require_map()
            S['res']['umap_before'] = os.path.getsize(UMAP)
            omb.load_layout()
            drop = omb.step_adits()
            S['drop'] = drop
            S['res']['drop_off_cm'] = [round(drop.x), round(drop.y), round(drop.z)]
            unreal.SystemLibrary.execute_console_command(omb.world(), 'RebuildNavigation')
            S['phase'], S['mark'] = 'nav', S['t']
            log('placed; navigation rebuild started')
        elif ph == 'nav' and S['t'] - S['mark'] > 8.0 and omb.nav_ready():
            S['phase'] = 'saving'          # the save ticks Slate again: never re-enter this branch
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                ore_nav_check.run()
            S['res']['nav_check'] = buf.getvalue().splitlines()
            S['res']['lorry_routes'] = lorry_routes(S['drop'])
            log('nav check done')
            ok = omb.save_level()
            S['res']['saved'] = bool(ok)
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
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, SHOTS + name + '.png', camera=S['cam'])
            log('shot ' + name)
        elif ph == 'quit' and S['t'] - S['mark'] > 8.0:
            quit_now()
    except Exception:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log('ERROR ' + traceback.format_exc())
        quit_now()


unreal.register_slate_post_tick_callback(tick)
