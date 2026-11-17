# One-off: derives rockwork_fire_test.py from the desert rock real-fire test (kept for the record).
# Changes: rockwork Blueprints, two rockwork slots, per-agent navigation (Character, LightTank, MediumTank, HeavyTank,
# each on its own navmesh with its own query filter, as rock_crush_test.py), material statistics read in the editor.
SRC = 'D:/UE5Projects/RTS_Survival/Source/Tools/DesertRocks/rock_fire_test.py'
p = 'D:/UE5Projects/RTS_Survival/Source/Tools/OreRockworks/rockwork_fire_test.py'
s = open(SRC, encoding='utf-8').read()


def rep(old, new, count=1):
    global s
    if old not in s:
        raise SystemExit('not found: ' + old[:90])
    s = s.replace(old, new) if count == 0 else s.replace(old, new, count)


a = s.index('import json')
s = '''"""Throwaway-editor test of the Ore rockworks (ue_blueprints.py): real tank fire, holes, death, navigation.
Derived from Tools/DesertRocks/rock_fire_test.py (same phases). Never in the shared editor; the map
(UnitTests/RTS_ToCopySmall) is never saved; the editor quits at the end. Launch: sh fire_test_run.sh <logname> [timeout_s].
Two rockworks stand at y 0 (yaw 90), one player Pz IV G 35 m in front of each (+Y). ORW_ONLY=<rock>,<rock> picks them.
Navigation: points inside the nav box, and a path across each rock for all four agents (own navmesh + own filter),
while alive and after death. Material statistics of M_OreRockwork and its _WallHoles copy are read here (the
commandlet reports 0). Results: Saved/OreMountains/rockworks/fire/result.json + PNGs.
"""
''' + s[a:]
rep("ROOT = 'D:/UE5Projects/RTS_Survival/Saved/DesertRocks/test/'", "ROOT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/rockworks/fire/'")
rep("BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/'",
    "BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Rockworks/Blueprints/'\n"
    "MATS = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Rockworks/Materials/'\n"
    "FILTERS = {a: f'/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_{n}.RTS_NavFilter_{n}_C'\n"
    "           for a, n in (('Character', 'Characters'), ('LightTank', 'LightTanks'), ('MediumTank', 'MediumTanks'),\n"
    "                        ('HeavyTank', 'HeavyTanks'))}")
i = s.index("_ONLY = [k for k in os.environ.get('DROCK_ONLY'")
j = s.index("RUINS = {k: v for k, v in _ALL.items()")
s = s[:i] + '''_ONLY = [k for k in os.environ.get('ORW_ONLY', '').split(',') if k] or ['R03_LeaningSlab', 'R04_SplitTor']
# (x, camera eye offset, look offset) per slot, world space
_SLOTS = [(-3000, (1100, 2700, 1200), (0, 0, 200)), (2500, (1100, 2700, 1200), (0, 0, 200))]
_ALL = {k: _SLOTS[i] for i, k in enumerate(_ONLY[:2])}
''' + s[j:]
rep("BP_DesertRock_", "BP_OreRockwork_", 0)
rep("unreal.log('DROCKTEST ' + message)", "unreal.log('ORWTEST ' + message)")
# per-agent navigation
rep('''def heavy_navmesh(world):''', '''def navmeshes(world):
    out = {}
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        for agent in ('LightTank', 'MediumTank', 'HeavyTank'):
            if agent in nav.get_name():
                out[agent] = nav
    rest = [n for n in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
            if n not in out.values()]
    if rest:
        out['Character'] = rest[0]
    return out


def agent_path(world, ends, nav_data, filter_class):
    """Path across the rock for one agent; side = how far the path leaves the straight line (world X, the rock
    stands at yaw 90 so the line runs along world Y)."""
    a, b = ends
    try:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b, nav_data, filter_class)
        pts = list(p.get_editor_property('path_points')) if p else []
    except Exception as error:  # noqa: BLE001
        return dict(error=repr(error)[:100])
    if not pts:
        return dict(length=None)
    length = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1))
    side = max(abs(pt.x - a.x) for pt in pts)
    return dict(length=round(length), straight=round((b - a).length()), partial=bool(p.is_partial()),
                side_cm=round(side), through=bool(side < 30 and not p.is_partial()))


def agent_paths(world, key):
    navs = S.get('navs') or navmeshes(world)
    S['navs'] = navs
    return {agent: agent_path(world, S['across'][key], nav, unreal.load_class(None, FILTERS[agent]))
            for agent, nav in sorted(navs.items())}


def heavy_navmesh(world):''')
rep('''    path = path_across(world, S['across'][key]) if key in S.get('across', {}) else 'n/a'
    heavy = path_across(world, S['across'][key], heavy_navmesh(world)) if key in S.get('across', {}) else 'n/a'
    log(f'nav {label} {key}: points inside nav boxes on navmesh {on_in}/{total_in} {leaky}; open points on navmesh '
        f'{sum(1 for v in open_on if v is True)}/{len(open_points)}; path across {path}; HeavyTank path {heavy}; '
        f'nav building {nav_building(world)}\'''', '''    agents = agent_paths(world, key) if key in S.get('across', {}) else 'n/a'
    S.setdefault('nav_result', {}).setdefault(key, {})[label] = agents
    log(f'nav {label} {key}: points inside nav boxes on navmesh {on_in}/{total_in} {leaky}; open points on navmesh '
        f'{sum(1 for v in open_on if v is True)}/{len(open_points)}; agent paths across {json.dumps(agents)}; '
        f'nav building {nav_building(world)}\'''')
# material statistics, read once PIE runs (shader maps exist in the editor)
rep('''                for key, ruin in S['ruins'].items():
                    report_setup(key, ruin)''', '''                for key, ruin in S['ruins'].items():
                    report_setup(key, ruin)
                for mat in ('M_OreRockwork', 'M_OreRockwork_WallHoles'):
                    try:
                        st = unreal.MaterialEditingLibrary.get_statistics(unreal.load_asset(MATS + mat))
                        log(f'material stats {mat}: pixel {st.num_pixel_shader_instructions} vertex '
                            f'{st.num_vertex_shader_instructions} samplers {st.num_samplers} texture samples '
                            f'{st.num_pixel_texture_samples}')
                    except Exception as error:  # noqa: BLE001
                        log(f'material stats {mat}: {error!r}')''')
rep('''        json.dump(S['log'], open(OUT, 'w'), indent=1)''', '''        json.dump(dict(log=S['log'], nav=S.get('nav_result', {})), open(OUT, 'w'), indent=1, default=str)''')
rep("    if S['t'] > 420.0 and S['phase'] != 'done':", "    if S['t'] > 520.0 and S['phase'] != 'done':")
# one extra 75 mm debug hit on the thin leaning slab (block 1 of R03; the real shells hit the support block), so the
# 57 cm hole cap on the thinnest block is exercised and pictured
rep('''                    if holes.get_hole_count() < 2 or key in NO_TANK:
                        debug_hits(key)''', '''                    if holes.get_hole_count() < 2 or key in NO_TANK:
                        debug_hits(key)
                    if key == 'R03_LeaningSlab':
                        slab_hit(ruin)''')
rep('''def debug_hits(key):''', '''def slab_hit(ruin):
    rocks = json.load(open('D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreRockworks/out/rocks.json'))
    c = rocks['R03_LeaningSlab']['part_info'][1]['centroid']
    target = ruin.get_actor_transform().transform_location(unreal.Vector(c[0] * 100, c[1] * 100, c[2] * 100 + 40))
    start = target + unreal.Vector(0, 3000, 600)
    direction = (target - start).normal()
    mesh = mesh_of(ruin)
    hit = mesh.line_trace_component(start, start + direction * 6000, False, False, False)
    if not hit:
        log('R03_LeaningSlab slab hit: missed the collision')
        return
    holes = ruin.get_component_by_class(unreal.WallHoleDamageComponent)
    removed = holes.debug_fire_test_hit(hit[0], direction, 75.0)
    local = ruin.get_actor_transform().inverse_transform_location(hit[0])
    log(f'R03_LeaningSlab DEBUG slab hit 75 mm at ({local.x:.0f},{local.y:.0f},{local.z:.0f}) removed {removed:.2f} m2, '
        f'holes {holes.get_hole_count()}')


def debug_hits(key):''')
open(p, 'w', encoding='utf-8').write(s)
left = [w for w in ('DesertRock', 'DROCK', 'L1_Block', 'M4_Cracked') if w in s]
print('ok, leftovers:', left)
