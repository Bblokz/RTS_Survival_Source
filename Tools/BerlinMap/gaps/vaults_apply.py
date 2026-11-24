"""Places BP_Berlin_BreweryVaults next to the brewery hall on Ger_Berlin and clears its footprint (throwaway editor,
through gaps_editor_server, after vaults_clear_plan.py). Only single instances are removed from the instanced actors;
the three loose actors standing inside the footprint are deleted. Nothing is saved here."""
import json
import math

VP = json.load(open(OUT + 'vaults_plan.json'))
KEEP_ACTORS = ('FOOT_HeapCover_Collapse_L_122',)        # only touches the margin
BP = '/Game/RTS_Survival/Blueprints/Environment/Berlin/BreweryEnclave/Blueprints/BP_Berlin_BreweryVaults'


def clear_and_place():
    check_map()
    vx, vy, vz, yaw = VP['vaults']
    ca, sa = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))

    def inside(x, y, m=0.0):
        lx, ly = (x - vx) * ca + (y - vy) * sa, -(x - vx) * sa + (y - vy) * ca
        return -1711.0 - m < lx < 1620.0 + m and -1620.0 - m < ly < 1842.0 + m

    by_label = {}
    for a in asub().get_all_level_actors():
        by_label.setdefault(a.get_actor_label(), []).append(a)
    assert 'BP_Berlin_BreweryVaults' not in by_label, 'vaults already placed'
    removed = 0
    for e in sorted(VP['instances'], key=lambda e: -e['k']):
        done = False
        for a in by_label.get(e['a'], []):
            for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
                if c.get_name() != e['c'] or e['k'] >= c.get_instance_count():
                    continue
                t = c.get_instance_transform(e['k'], True).translation
                if abs(t.x - e['xy'][0]) > 5 or abs(t.y - e['xy'][1]) > 5:
                    continue
                before = c.get_instance_count()
                c.remove_instance(e['k'])
                print('removed instance %s #%d (%s): %d -> %d left in that actor' % (e['a'], e['k'], e['mesh'], before, c.get_instance_count()))
                removed += 1
                done = True
                break
            if done:
                break
        assert done, 'instance not found: %s' % e
    deleted = []
    for e in VP['actors']:
        if e['a'] in KEEP_ACTORS:
            continue
        for a in by_label.get(e['a'], []):
            loc = a.get_actor_location()
            if abs(loc.x - e['xy'][0]) < 400 and abs(loc.y - e['xy'][1]) < 400:
                deleted.append(e['a'])
                asub().destroy_actor(a)
    extra = {}
    for a in asub().get_all_level_actors():
        loc = a.get_actor_location()
        if inside(loc.x, loc.y) and not isinstance(a, (unreal.LandscapeProxy, unreal.Volume)) and abs(loc.z - vz) < 800:
            cls = a.get_class().get_name()
            if isinstance(a, unreal.DecalActor):
                extra['decal'] = extra.get('decal', 0) + 1
                asub().destroy_actor(a)
            else:
                print('left inside the footprint: %s (%s)' % (a.get_actor_label(), cls))
    cls = unreal.EditorAssetLibrary.load_blueprint_class(BP)
    v = asub().spawn_actor_from_class(cls, unreal.Vector(vx, vy, vz), unreal.Rotator(0.0, 0.0, yaw))
    v.set_actor_label('BP_Berlin_BreweryVaults')
    print('removed %d instances, deleted actors %s, decals inside removed %s' % (removed, deleted, extra))
    print('vaults', v.get_actor_label(), v.get_actor_location(), v.get_actor_rotation(), v.get_actor_scale3d())
