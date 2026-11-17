"""Applies gaps_scan.py's plan to Ger_Berlin in the throwaway editor (exec through gaps_editor_server after
gaps_dump.py). import_heights(): the raised landscape (sub-rectangle, paint layers written back unchanged).
apply_drops(): lowers / lifts the loose pieces (actors as a whole, instances one by one). Nothing is saved here."""
import json

PLAN = json.load(open(OUT + 'plan.json'))


def import_heights():
    check_map()
    land = landscape()
    names = open(OUT + 'grid.txt').read().splitlines()[3].split()
    tl = land.get_editor_property('target_layers')
    by_name = {str(k): tl[k].get_editor_property('layer_info_obj') for k in tl.keys()}
    i0, j0, i1, j1 = PLAN['bbox']
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(
        land, i0, j0, i1, j1, OUT + 'sub_height.r16', [by_name[n] for n in names],
        [OUT + 'sub_%s.r8' % n for n in names])
    print('heights (%d,%d)-(%d,%d): %s' % (i0, j0, i1, j1, ok))
    assert ok


def apply_drops():
    check_map()
    actors = {}
    for a in asub().get_all_level_actors():
        actors.setdefault(a.get_actor_label(), []).append(a)
    moved_actors, moved_inst, missing = set(), 0, 0
    for d in PLAN['drops']:
        found = actors.get(d['a'], [])
        if d['k'] < 0:
            # several actors can share a label: the one at the planned place
            best = [a for a in found if abs(a.get_actor_location().x - d['xy'][0]) < 400 and abs(a.get_actor_location().y - d['xy'][1]) < 400]
            if not best:
                missing += 1
                continue
            a = best[0]
            if a in moved_actors:
                continue
            loc = a.get_actor_location()
            a.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z - d['drop']), False, False)
            moved_actors.add(a)
        else:
            done = False
            for a in found:
                for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
                    if c.get_name() != d['c'] or d['k'] >= c.get_instance_count():
                        continue
                    t = c.get_instance_transform(d['k'], True)
                    if abs(t.translation.x - d['xy'][0]) > 5 or abs(t.translation.y - d['xy'][1]) > 5:
                        continue
                    t.translation = unreal.Vector(t.translation.x, t.translation.y, t.translation.z - d['drop'])
                    c.update_instance_transform(d['k'], t, True, True, True)
                    moved_inst += 1
                    done = True
                    break
                if done:
                    break
            if not done:
                missing += 1
    print('moved actors %d, instances %d, not found %d (of %d plan rows)' % (len(moved_actors), moved_inst, missing, len(PLAN['drops'])))
