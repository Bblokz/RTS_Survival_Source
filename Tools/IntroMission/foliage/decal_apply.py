"""Cliff stone decal actors from Saved/IntroFoliage/decals/decals_r<N>.json (exec'd in the throwaway editor).

FOL_StoneDecal_<Name>_<k> DecalActors in folder IntroFoliage/CliffStones: pitch -90 (projects straight down, like
every ground decal on this map), DecalSize = (half box depth, half height, half width) in cm.
UV orientation (PIE calibration 2026-11-20, shots/t5_decal_calib.png): with pitch -90 the texture's U (width) runs along
local Z = world (cos yaw, sin yaw), V along local Y; the image top landed on -V, so YAW_FIX = 180 puts the image top on
the plan's +V = (-sin yaw, cos yaw), which the planner points at the cliff for ScreeFan / RubbleStrip.
"""
import json

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroFoliage/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/CliffStones/'
PREFIX = 'FOL_StoneDecal_'
DEPTH_HALF = 60.0          # review r4: +-60 cm; the planner keeps the ground under the rectangle within +-45 cm
                           # (thinner box = less stone paint on a tank parked on a decal)
YAW_FIX = 180.0


def _eas():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def clear():
    n = 0
    for a in _eas().get_all_level_actors():
        if a.get_actor_label().startswith(PREFIX):
            _eas().destroy_actor(a)
            n += 1
    print('cleared', n, PREFIX)


def _ground(x, y):
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    hits = unreal.SystemLibrary.line_trace_multi(w, unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    for h in hits or []:
        t = h.to_tuple()
        actor = t[9] if len(t) > 9 else None
        if actor is not None and actor.get_class().get_name().startswith('Landscape'):
            return t[4].z
    return None


def spawn(d, k, mats):
    z = _ground(d['x'], d['y'])
    if z is None:
        return None
    a = _eas().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(d['x'], d['y'], z),
                                      unreal.Rotator(0.0, -90.0, d['yaw'] + YAW_FIX))
    a.set_actor_label('%s%s_%03d' % (PREFIX, d['name'], k))
    a.set_folder_path('IntroFoliage/CliffStones')
    c = a.get_editor_property('decal')
    c.set_decal_material(mats[d['name']])
    c.set_editor_property('decal_size', unreal.Vector(DEPTH_HALF, d['h_m'] * 50.0, d['w_m'] * 50.0))
    c.set_editor_property('sort_order', 1)            # over the map's dirt / crater decals (sort 0)
    c.set_editor_property('fade_screen_size', 0.002)
    return a


def apply(round_no, only=None):
    plan = json.load(open(ROOT + 'decals/decals_r%d.json' % round_no))
    clear()
    mats = {}
    made = 0
    for k, d in enumerate(plan['decals']):
        if only is not None and k not in only:
            continue
        if d['name'] not in mats:
            mats[d['name']] = unreal.load_asset(HOME + 'MI_CliffStoneDecal_' + d['name'])
        made += spawn(d, k, mats) is not None
    print('stone decals', made, 'of', len(plan['decals']))
