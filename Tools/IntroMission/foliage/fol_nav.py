"""Nav routes of the Ger_IntroMission missions, per RecastNavMesh agent (exec'd in the throwaway editor via fol_cmd.py).

The foliage + POI pass keeps clear of these routes (they are where tanks and squads actually drive), and the final
check compares every route length with the baseline taken before anything was placed.

    exec(open(FOL + 'fol_nav.py').read()); nav_routes('baseline')  -> Saved/IntroFoliage/nav_<tag>.json
"""
import json
import math

import unreal

NAV_OUT = 'D:/UE5Projects/RTS_Survival/Saved/IntroFoliage/'

# (name, start, end): the mission's real movements (docs GerIntroMission.md, SovietCounterattackMission.md).
ROUTES = [
    ('player_to_moveto', (-30807, -2496), (-15697, -1275)),
    ('moveto_to_sandbags', (-15697, -1275), (-13800, -1300)),
    ('hangar1_to_t26target', (-11400, -2416), (-21200, 3500)),
    ('hangar3_to_t26target', (-13016, -8800), (-21200, 3500)),
    ('flank_t26_a', (-5780, -24300), (-14500, -1300)),
    ('flank_t26_b', (-6180, -21900), (-15500, -500)),
    ('reinforcements_to_gate', (-30984, 3366), (-4400, -300)),
    ('moveto_to_ridge_ramp', (-15697, -1275), (-6800, 0)),
    ('ramp_to_base', (-6800, 0), (800, 500)),
    ('glider_to_hq', (-4500, 300), (1450, 300)),
    ('counter_centre_to_hq', (8400, -1500), (1450, 300)),
    ('counter_pad1_to_hq', (20446, -16317), (1450, 300)),
    ('counter_pad4_to_hq', (16154, -15083), (1450, 300)),
    ('counter_station_to_hq', (19750, 10100), (1450, 300)),
    ('fuel_truck_lane', (4640, -5400), (4640, -1717)),
    ('player_to_hangar3', (-30807, -2496), (-13016, -9000)),
    ('player_to_lakeside', (-30807, -2496), (-24000, -12000)),
    ('hq_to_station', (1450, 300), (19650, 10200)),
    ('hq_to_depot', (1450, 300), (18300, -15700)),
]


def _w():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _gz(x, y):
    hit = unreal.SystemLibrary.line_trace_single(_w(), unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else 150.0


def _navs():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return {a.get_actor_label(): a for a in sub.get_all_level_actors() if a.get_class().get_name() == 'RecastNavMesh'}


def nav_routes(tag):
    out = {}
    for label, nav in sorted(_navs().items()):
        for name, s, e in ROUTES:
            p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
                _w(), unreal.Vector(s[0], s[1], _gz(*s) + 60.0), unreal.Vector(e[0], e[1], _gz(*e) + 60.0), nav)
            pts = [(round(q.x), round(q.y)) for q in p.get_editor_property('path_points')] if p else []
            length = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts[:-1], pts[1:])) / 100.0
            reached = bool(pts) and math.hypot(pts[-1][0] - e[0], pts[-1][1] - e[1]) < 800
            out.setdefault(label, {})[name] = {'len_m': round(length, 1), 'reached': reached, 'pts': pts}
            print('%-28s %-24s %7.1f m %s' % (label, name, length, 'ok' if reached else 'NOT REACHED'))
    with open(NAV_OUT + 'nav_%s.json' % tag, 'w') as h:
        json.dump(out, h)
    return out
