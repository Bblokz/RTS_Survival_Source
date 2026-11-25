"""Read-only dump of Ger_2ndOreMountains for the site / exploration placement jobs. Saves nothing.

Commandlet (PowerShell, Development, no GPU needed):
    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -NoSound
        -NoLiveCoding -DisablePlugins=Fab,BpGeneratorUltimate -abslog=<log>
Set ORE_VERIFY_TAG (env) to name the report: Saved/OreMountains/stages/verify_<tag>.json (default "now").
Reports: counts per mission tag, the convoy routes (RouteId, Ambush.RadioPostActor), the pressure director's
TargetedAttack targets, the artillery battery id, strongpoint ids, every ORE_Site_* / ORE_Explore_* / checkpoint /
placeholder actor of interest (class, location, yaw, tags), metal resources near the heap stations, and nav data.
"""
import json
import math
import os

import unreal

MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains'
OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/stages/'
TAGS = ['ORE_BaseA_Target', 'ORE_BaseB_Target', 'ORE_RadioOutpost', 'ORE_BaseB_RadioMast', 'ORE_SovietSite',
        'ORE_Explore', 'ORE_Site_Border', 'ORE_Director']
PREFIXES = ('ORE_Site_', 'ORE_BaseB_', 'ORE_Explore_', 'ORE_CP_Barrier', 'ORE_PH_wismut', 'ORE_Note_PH_wismut',
            'ORE_PH_cp_', 'ORE_Prop_Yard_Stack_3', 'ORE_PressureDirector', 'ORE_Convoy', 'ORE_Battery',
            'ORE_HQ_AttackMarker')


def log(text):
    unreal.log_warning('ORE_VERIFY ' + str(text))


def label_of(actor):
    return actor.get_actor_label() if actor is not None else None


def row(actor):
    loc = actor.get_actor_location()
    rot = actor.get_actor_rotation()
    out = {'label': actor.get_actor_label(), 'class': actor.get_class().get_name(),
           'loc': [round(loc.x), round(loc.y), round(loc.z)], 'yaw': round(rot.yaw, 1),
           'tags': [str(t) for t in actor.get_editor_property('tags')]}
    if abs(rot.pitch) > 0.05 or abs(rot.roll) > 0.05:
        out['pitch_roll'] = [round(rot.pitch, 2), round(rot.roll, 2)]
    parent = actor.get_attach_parent_actor()
    if parent is not None:
        out['attached_to'] = parent.get_actor_label()
    return out


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    if world is None:
        log('FAIL could not load ' + MAP)
        return
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    report = {'map': world.get_path_name(), 'actor_count': len(actors)}
    labels = {}
    for a in actors:
        labels.setdefault(a.get_actor_label(), []).append(a)
    report['duplicate_ore_labels'] = sorted(l for l, v in labels.items() if l.startswith('ORE_') and len(v) > 1)
    report['tags'] = {}
    for tag in TAGS:
        hits = sorted(a.get_actor_label() for a in actors if tag in [str(t) for t in a.get_editor_property('tags')])
        report['tags'][tag] = {'count': len(hits), 'labels': hits if tag not in ('ORE_SovietSite', 'ORE_Explore') else []}
    routes = []
    for a in actors:
        if isinstance(a, unreal.EnemyConvoyRoute):
            ambush = a.get_editor_property('ambush')
            post = ambush.get_editor_property('radio_post_actor')
            routes.append({'label': a.get_actor_label(), 'route_id': str(a.get_editor_property('route_id')),
                           'radio_post_actor': label_of(post),
                           'radio_post_class': post.get_class().get_name() if post is not None else None})
    report['routes'] = sorted(routes, key=lambda r: r['label'])
    directors = []
    for a in actors:
        if isinstance(a, unreal.ConvoyPressureDirector):
            tiers = []
            for t in a.get_editor_property('tier_responses'):
                attack = t.get_editor_property('targeted_attack')
                tiers.append({'name': str(t.get_editor_property('tier_name')),
                              'start': [label_of(x) for x in t.get_editor_property('placement_actors_to_start')],
                              'attack_enabled': attack.get_editor_property('enabled'),
                              'targets': [label_of(x) for x in attack.get_editor_property('targets')],
                              'target_classes': [x.get_class().get_name() if x is not None else None
                                                 for x in attack.get_editor_property('targets')]})
            cargo = [{'cargo': str(c.get_editor_property('cargo_type')),
                      'start': [label_of(x) for x in c.get_editor_property('placement_actors_to_start')]}
                     for c in a.get_editor_property('cargo_responses')]
            directors.append({'label': a.get_actor_label(), 'tiers': tiers, 'cargo': cargo})
    report['directors'] = directors
    report['batteries'] = [{'label': a.get_actor_label(), 'id': str(a.get_editor_property('battery_id'))}
                           for a in actors if isinstance(a, unreal.EnemyArtilleryBattery)]
    report['strongpoints'] = sorted([a.get_actor_label(), str(a.get_editor_property('strongpoint_id')),
                                     len(a.get_editor_property('posts'))]
                                    for a in actors if isinstance(a, unreal.EnemyStrongpoint))
    report['actors'] = sorted((row(a) for a in actors if a.get_actor_label().startswith(PREFIXES)),
                              key=lambda r: r['label'])
    stations = [a for a in actors if a.get_actor_label().startswith('ORE_Explore_Station_')]
    near = []
    for st in stations:
        sl = st.get_actor_location()
        for a in actors:
            if 'Resource_Metal' in a.get_class().get_name():
                loc = a.get_actor_location()
                d = math.hypot(loc.x - sl.x, loc.y - sl.y)
                if d < 5000:
                    origin, extent = a.get_actor_bounds(True)
                    near.append({'station': st.get_actor_label(), 'label': a.get_actor_label(),
                                 'class': a.get_class().get_name(), 'loc': [round(loc.x), round(loc.y), round(loc.z)],
                                 'dist_m': round(d / 100.0, 1),
                                 'bounds_c': [round(origin.x), round(origin.y)], 'bounds_e': [round(extent.x), round(extent.y)]})
    report['station_metal'] = near
    report['nav_data'] = [a.get_class().get_name() for a in actors if 'NavMesh' in a.get_class().get_name()]
    report['mission_managers'] = sorted(a.get_actor_label() for a in actors if 'MissionManager' in a.get_class().get_name())
    tag = os.environ.get('ORE_VERIFY_TAG', 'now')
    os.makedirs(OUT_DIR, exist_ok=True)
    path = OUT_DIR + 'verify_%s.json' % tag
    with open(path, 'w') as handle:
        json.dump(report, handle, indent=1)
    log('wrote %s: %d actors, tags %s, routes %s, directors %d' % (
        path, len(actors), {k: v['count'] for k, v in report['tags'].items()},
        [(r['route_id'], r['radio_post_actor']) for r in report['routes']], len(directors)))


main()
