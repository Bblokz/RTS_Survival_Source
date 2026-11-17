"""Read-only survey of Ger_Berlin for the horizon skirt (exec through skirt_editor_server with the map open).

Functions (each writes into Saved/MapSkirt/Ger_Berlin/, nothing is changed or saved):
  survey()            survey.txt (landscape, volumes, fog of war, nav actors), actors.tsv, blockers.tsv, instances.tsv
  export(tag)         <tag>_height.r16 + <tag>_<Layer>.r8 + grid.txt (URTSShortsLandscapeLibrary, DebugGame build)
  void_grid(tag)      void_<tag>.json: 10 m trace grid over +-1500 m (L landscape, M mesh, V nothing) + hit heights;
                      cells under a SKIRT_Plane_* footprint are marked S (the planes have no collision)
  nav_grid(tag)       nav_<tag>.json: per nav agent, which 10 m grid points project onto the navmesh (+-620 m)
  counts(tag)         counts_<tag>.json: actors per class and per label prefix (SKIRT_* listed apart)
"""
import collections
import json
import os
import struct

import unreal

MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/Ger_Berlin/'
os.makedirs(OUT, exist_ok=True)
PLAY_X, PLAY_Y = 26000.0, 50000.0


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def asub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def check_map():
    path = world().get_path_name()
    assert path.startswith(MAP), 'wrong map open: ' + path
    return path


def landscape():
    lands = [a for a in asub().get_all_level_actors() if isinstance(a, unreal.Landscape)]
    assert len(lands) == 1, 'landscapes: %d' % len(lands)
    return lands[0]


def nav_building():
    w = world()
    return unreal.NavigationSystemV1.get_navigation_system(w).is_navigation_being_built(w)


def _mat_name(a):
    try:
        if isinstance(a, unreal.StaticMeshActor):
            m = a.static_mesh_component.get_material(0)
            sm = a.static_mesh_component.static_mesh
            return (m.get_name() if m else '-') + '|' + (sm.get_name() if sm else '-') + '|' + \
                a.static_mesh_component.get_collision_enabled().name
    except Exception:  # noqa
        pass
    return '-'


def survey():
    lines = ['map %s' % check_map()]
    acts = asub().get_all_level_actors()
    lines.append('actors %d  nav building %s' % (len(acts), nav_building()))
    rows, blk, inst = [], [], []
    for a in acts:
        c, l = a.get_class().get_name(), a.get_actor_label()
        o, e = a.get_actor_bounds(False)
        p = a.get_actor_location()
        rows.append('\t'.join([l, c, '%.0f' % p.x, '%.0f' % p.y, '%.0f' % p.z, '%.0f' % o.x, '%.0f' % o.y, '%.0f' % o.z,
                               '%.0f' % e.x, '%.0f' % e.y, '%.0f' % e.z, str(a.get_folder_path()), _mat_name(a)]))
        row = '%s | %s | loc (%.0f,%.0f,%.0f) | o (%.0f,%.0f,%.0f) e (%.0f,%.0f,%.0f) | scale %s' % (
            c, l, p.x, p.y, p.z, o.x, o.y, o.z, e.x, e.y, e.z, a.get_actor_scale3d().to_tuple())
        if isinstance(a, unreal.LandscapeProxy):
            lines.append('LANDSCAPE ' + row)
            for prop in ('landscape_material', 'can_have_layers_content', 'runtime_virtual_textures',
                         'virtual_texture_render_pass_type'):
                try:
                    lines.append('   %s = %s' % (prop, a.get_editor_property(prop)))
                except Exception as ex:  # noqa
                    lines.append('   %s ? %s' % (prop, ex))
            try:
                tl = a.get_editor_property('target_layers')
                lines.append('   layers = %s' % [str(k) for k in tl.keys()])
            except Exception as ex:  # noqa
                lines.append('   layers ? %s' % ex)
            lines.append('   components %d' % len(a.get_components_by_class(unreal.LandscapeComponent)))
            lines.append('   grid %s' % unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(a))
        elif c in ('BlockingVolume', 'NavMeshBoundsVolume', 'RuntimeVirtualTextureVolume') or 'FogOfWar' in c \
                or 'RecastNavMesh' in c or 'NavBlocker' in c or 'NavModifier' in c:
            lines.append('VOL ' + row)
            if 'FogOfWar' in c:
                lines.append('   map_extent = %s' % a.get_editor_property('map_extent'))
            if c == 'BlockingVolume':
                blk.append('\t'.join([l] + ['%.0f' % v for v in (o.x, o.y, o.z, e.x, e.y, e.z)]))
        elif c in ('DirectionalLight', 'SkyLight', 'ExponentialHeightFog', 'SkyAtmosphere', 'PostProcessVolume',
                   'VolumetricCloud') or 'Sky' in c:
            lines.append('LIGHT ' + row)
        elif any(s in (c + l).lower() for s in ('skirt', 'horizon', 'backdrop', 'water', 'spree', 'rail', 'track')):
            lines.append('EDGE? ' + row + ' | ' + _mat_name(a))
        # instanced meshes outside the play area: one row per instance (the actor bounds of these are map-wide)
        for comp in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            n = comp.get_instance_count()
            sm = comp.static_mesh
            if not n or not sm:
                continue
            b = sm.get_bounds()
            rad = max(b.box_extent.x, b.box_extent.y)
            col = comp.get_collision_enabled().name
            for i in range(n):
                t = comp.get_instance_transform(i, True)
                if isinstance(t, tuple):
                    t = t[-1]
                if t is None:
                    continue
                q = t.translation
                if abs(q.x) < PLAY_X - 1000 and abs(q.y) < PLAY_Y - 1000:
                    continue
                s = t.scale3d
                inst.append('\t'.join([l, sm.get_name(), '%.0f' % q.x, '%.0f' % q.y, '%.0f' % q.z,
                                       '%.0f' % (rad * max(abs(s.x), abs(s.y))), col]))
    w = world()
    ns = unreal.NavigationSystemV1.get_navigation_system(w)
    lines.append('nav system %s building %s' % (ns, nav_building()))
    with open(OUT + 'survey.txt', 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    with open(OUT + 'actors.tsv', 'w', encoding='utf-8') as f:
        f.write('\n'.join(rows))
    with open(OUT + 'blockers.tsv', 'w', encoding='utf-8') as f:
        f.write('\n'.join(blk))
    with open(OUT + 'instances.tsv', 'w', encoding='utf-8') as f:
        f.write('\n'.join(inst))
    print('survey: %d actors, %d blockers, %d outer instances, %d survey lines' % (len(rows), len(blk), len(inst), len(lines)))


def export(tag='full'):
    check_map()
    land = landscape()
    grid = [int(v) for v in unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(land).split()]
    tl = land.get_editor_property('target_layers')
    names = [str(k) for k in tl.keys()]
    infos = [tl[k].get_editor_property('layer_info_obj') for k in tl.keys()]
    ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(
        land, grid[0], grid[1], grid[2], grid[3], OUT + '%s_height.r16' % tag, infos,
        [OUT + '%s_%s.r8' % (tag, n) for n in names])
    loc, sc = land.get_actor_location(), land.get_actor_scale3d()
    with open(OUT + 'grid.txt', 'w') as f:
        f.write('%d %d %d %d %d\n%f %f %f\n%f %f %f\n%s\n' % (grid[0], grid[1], grid[2], grid[3], grid[4], loc.x, loc.y,
                                                         loc.z, sc.x, sc.y, sc.z, ' '.join(names)))
    print('export %s ok=%s grid=%s layers=%s' % (tag, ok, grid, names))


def _skirt_rects():
    rects = []
    for a in asub().get_all_level_actors():
        if a.get_actor_label().startswith('SKIRT_Plane_') or a.get_actor_label().startswith('SKIRT_Water_'):
            o, e = a.get_actor_bounds(False)
            rects.append((o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y))
    return rects


def void_grid(tag='before', reach=150000.0, step=1000.0):
    check_map()
    w = world()
    rects = _skirt_rects()
    n = int(2 * reach / step) + 1
    rows, heights = [], []
    cnt = collections.Counter()
    for i in range(n):
        x = -reach + i * step
        row = []
        for j in range(n):
            y = -reach + j * step
            hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 60000), unreal.Vector(x, y, -30000),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                         unreal.DrawDebugTrace.NONE, True)
            z = -99999.0
            if hit is None:
                ch = 'V'
            else:
                t = hit.to_tuple()
                if not t[0]:
                    ch = 'V'
                else:
                    a = t[9]
                    ch = 'L' if a and a.get_class().get_name() == 'Landscape' else 'M'
                    z = t[4].z
            if ch == 'V' and any(r[0] <= x <= r[1] and r[2] <= y <= r[3] for r in rects):
                ch = 'S'
            row.append(ch)
            heights.append(z)
            cnt[ch] += 1
        rows.append(''.join(row))
    json.dump({'x0': -reach, 'y0': -reach, 'step': step, 'n': n, 'rows': rows, 'skirt_rects': rects},
              open(OUT + 'void_%s.json' % tag, 'w'))
    with open(OUT + 'void_%s_z.f32' % tag, 'wb') as f:
        f.write(struct.pack('%df' % len(heights), *heights))
    print('void grid %s: %d x %d cells, counts %s' % (tag, n, n, dict(cnt)))


def nav_grid(tag='before', reach=62000.0, step=1000.0):
    check_map()
    w = world()
    if nav_building():
        print('NAV STILL BUILDING: no grid written')
        return
    navs = [a for a in asub().get_all_level_actors() if 'RecastNavMesh' in a.get_class().get_name()]
    res = {'x0': -reach, 'y0': -reach, 'step': step, 'agents': {}}
    n = int(2 * reach / step) + 1
    ext = unreal.Vector(30.0, 30.0, 600.0)
    for nav in navs:
        rows, hits = [], 0
        for i in range(n):
            row = []
            for j in range(n):
                p = unreal.Vector(-reach + i * step, -reach + j * step, 0.0)
                hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(p.x, p.y, 60000), unreal.Vector(p.x, p.y, -30000),
                                                             unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                             unreal.DrawDebugTrace.NONE, True)
                if hit is not None and hit.to_tuple()[0]:
                    p.z = hit.to_tuple()[4].z
                r = unreal.NavigationSystemV1.project_point_to_navigation(w, p, nav, None, ext)
                ok = r is not None and not (isinstance(r, tuple) and not r[0]) and r is not False
                row.append('1' if ok else '0')
                hits += 1 if ok else 0
            rows.append(''.join(row))
        o, e = nav.get_actor_bounds(False)
        res['agents'][nav.get_name()] = {'rows': rows, 'on_nav': hits, 'bounds': [o.x, o.y, o.z, e.x, e.y, e.z]}
        print('nav %s: %d of %d grid points on the navmesh, bounds o (%.0f,%.0f,%.0f) e (%.0f,%.0f,%.0f)' % (
            nav.get_name(), hits, n * n, o.x, o.y, o.z, e.x, e.y, e.z))
    json.dump(res, open(OUT + 'nav_%s.json' % tag, 'w'))


def counts(tag='before'):
    check_map()
    by_class, by_prefix = collections.Counter(), collections.Counter()
    skirt = []
    for a in asub().get_all_level_actors():
        l = a.get_actor_label()
        if l.startswith('SKIRT_'):
            skirt.append(l)
            continue
        by_class[a.get_class().get_name()] += 1
        by_prefix[l.split('_')[0]] += 1
    total = sum(by_class.values())
    json.dump({'total_non_skirt': total, 'skirt': sorted(skirt), 'by_class': dict(by_class), 'by_prefix': dict(by_prefix)},
              open(OUT + 'counts_%s.json' % tag, 'w'), indent=1, sort_keys=True)
    print('counts %s: %d actors without SKIRT_, %d SKIRT_ actors, %d classes' % (tag, total, len(skirt), len(by_class)))
