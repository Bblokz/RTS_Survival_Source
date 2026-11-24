"""Throwaway-editor run for the shoreline foam on the Ger_IntroMission pond. Never run in the shared editor.

  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Mode is read from Saved/WaterShore/mode.txt:
  shoot -> find pond(s) on M_RTS_Water, log depth profile, swap in MI_Water_Shore (not saved), HighResShot views.
  save  -> swap in MI_Water_Shore, wait for the navmesh rebuild, save the map, quit.
"""
import json
import math
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WaterShore/'
OLD = '/Game/RTS_Survival/Materials/water/JungleWater/Masters/M_RTS_Water.M_RTS_Water'
NEW = '/Game/RTS_Survival/Materials/water/JungleWater/MI_Water_Shore.MI_Water_Shore'
try:
    MODE = open(ROOT + 'mode.txt').read().strip()
except Exception:  # noqa: BLE001
    MODE = 'shoot'
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'views': [], 'shots': 0}


def log(message):
    S['log'].append(message)
    unreal.log('WATERSHORE ' + message)


def finish():
    json.dump(S['log'], open(ROOT + 'result_' + MODE + '.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ponds():
    out = []
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            for i in range(c.get_num_materials()):
                m = c.get_material(i)
                if m and m.get_path_name() in (OLD, NEW):
                    out.append((a, c, i))
    return out


def ground_z(w, x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


def survey(w, actor, comp):
    org, ext = actor.get_actor_bounds(False)
    water_z = org.z + ext.z  # top of the plane
    step = max(ext.x, ext.y) / 20.0
    cells, shore = [], []
    nx, ny = int(ext.x * 2 / step), int(ext.y * 2 / step)
    for i in range(nx + 1):
        for j in range(ny + 1):
            x, y = org.x - ext.x + i * step, org.y - ext.y + j * step
            gz = ground_z(w, x, y, [actor])
            if gz is None:
                continue
            d = water_z - gz
            cells.append((x, y, d))
            if 0 < d < 60:
                shore.append((x, y, d))
    under = [c[2] for c in cells if c[2] > 0]
    log(f'pond {actor.get_actor_label()} mesh {comp.static_mesh.get_name()} bounds {org} ext {ext} water_z {water_z:.0f}')
    if under:
        under.sort()
        log(f'depth under water: n={len(under)} max={under[-1]:.0f} p25={under[len(under)//4]:.0f} '
            f'p50={under[len(under)//2]:.0f} p75={under[3*len(under)//4]:.0f}; shore cells(<60cm)={len(shore)}')
    # Horizontal shore gradient: for shore cells, how far to a cell 150 cm deeper.
    return org, ext, water_z, cells, shore


def plan_views(org, ext, water_z, shore):
    span = max(ext.x, ext.y)
    views = [('overview', unreal.Vector(org.x - span * 0.7, org.y - span * 0.7, water_z + span * 0.9),
              unreal.Vector(org.x, org.y, water_z))]
    if shore:
        # Two bank close-ups: shore cells farthest apart.
        a = shore[0]
        b = max(shore, key=lambda c: (c[0] - a[0]) ** 2 + (c[1] - a[1]) ** 2)
        a = max(shore, key=lambda c: (c[0] - b[0]) ** 2 + (c[1] - b[1]) ** 2)
        for name, p in (('bankA', a), ('bankB', b)):
            to_centre = unreal.Vector(org.x - p[0], org.y - p[1], 0)
            to_centre.normalize()
            target = unreal.Vector(p[0], p[1], water_z)
            eye = target + to_centre * 1400 + unreal.Vector(0, 0, 1100)
            views.append((name, eye, target))
    return views


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 20.0:
            w = world()
            log(f'map {w.get_path_name()} mode {MODE}')
            found = ponds()
            if not found:
                log('no pond with M_RTS_Water found')
                finish()
                return
            mi = unreal.load_asset(NEW)
            S['slots'] = []
            for actor, comp, idx in found:
                org, ext, wz, cells, shore = survey(w, actor, comp)
                S['slots'].append((comp, idx, comp.get_material(idx)))
                comp.set_material(idx, mi)
                S['views'] += plan_views(org, ext, wz, shore)
            S['new_mi'] = mi
            log(f'swapped {len(found)} slot(s) to MI_Water_Shore')
            if MODE == 'save':
                S['phase'], S['mark'] = 'wait_nav', S['t']
            else:  # 'shoot' or 'shoot_save' (shoot, then save)
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
                S['phase'], S['mark'] = 'warm', S['t']
        elif phase == 'warm' and S['t'] - S['mark'] > 70.0:
            S['phase'], S['mark'] = 'aim', S['t']
        elif phase == 'aim':
            if not S['views']:
                if MODE == 'shoot_save':
                    for comp, idx, old in S['slots']:
                        comp.set_material(idx, S['new_mi'])
                    S['phase'], S['mark'] = 'wait_nav', S['t']
                else:
                    finish()
                return
            name, eye, target = S['views'][0]
            rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye, rot)
            S['phase'], S['mark'], S['frame'] = 'shoot', S['t'], 0
        elif phase == 'shoot' and S['t'] - S['mark'] > 4.0 + S['frame'] * 1.0:
            name = S['views'][0][0]
            # Frame 0 = original material (baseline), frames 1.. = shore material.
            for comp, idx, old in S['slots']:
                comp.set_material(idx, old if S['frame'] == 0 else S['new_mi'])
            if S['frame'] in (0, 1):
                S['mark'] += 1.0  # give the swap a moment to render before the next shot
            unreal.SystemLibrary.execute_console_command(world(), 'HighResShot 1600x900')
            log(f'shot {name} frame {S["frame"]} ({"orig" if S["frame"] == 0 else "shore"}) t={S["t"]:.1f}')
            S['frame'] += 1
            if S['frame'] >= 4:
                S['views'].pop(0)
                S['phase'], S['mark'] = 'aim', S['t'] + 2.0
        elif phase == 'wait_nav':
            w = world()
            building = unreal.NavigationSystemV1.is_navigation_being_built(w)
            navs = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh)]
            nav_ok = all(a.get_actor_bounds(False)[1].length() > 0 for a in navs)
            if S['t'] - S['mark'] > 30.0 and not building and nav_ok:
                log(f'navmesh settled ({len(navs)} recast) after {S["t"] - S["mark"]:.0f}s')
                for comp, idx, old in S['slots']:
                    log(f'slot {comp.get_owner().get_actor_label()}[{idx}] = {comp.get_material(idx).get_name()}')
                # Saving ticks Slate again, which re-enters this callback: block re-entry.
                S['busy'] = True
                try:
                    ok = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
                finally:
                    S['busy'] = False
                log(f'saved level: {ok}')
                finish()
            elif S['t'] - S['mark'] > 400.0:
                log('navmesh never settled; not saved')
                finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
