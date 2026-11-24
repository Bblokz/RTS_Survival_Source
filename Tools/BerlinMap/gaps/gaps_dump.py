"""Ger_Berlin floating-gap scan, editor side (exec through gaps_editor_server in a throwaway DebugGame editor).

  dump_landscape()   full height export (r16) + the six paint layers + grid.txt       -> Saved/Berlin/gaps/
  dump_components()  every visible static mesh component / instance with its world transform -> components.json
  dump_meshes()      LOD0 geometry of every mesh used (vertices + triangles, raw float32 / int32) -> mesh/<n>.bin

Nothing on the map is changed here. The offline part is gaps_scan.py.
"""
import array
import json
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/gaps/'
MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin'
os.makedirs(OUT + 'mesh', exist_ok=True)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def asub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def check_map():
    path = world().get_path_name()
    assert path.startswith(MAP), 'wrong map open: ' + path


def landscape():
    lands = [a for a in asub().get_all_level_actors() if isinstance(a, unreal.Landscape)]
    assert len(lands) == 1
    return lands[0]


def dump_landscape(tag='full'):
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


def _xf(t):
    l, r, s = t.translation, t.rotation, t.scale3d
    return [l.x, l.y, l.z, r.x, r.y, r.z, r.w, s.x, s.y, s.z]


def dump_components():
    check_map()
    rows, meshes = [], {}
    for a in asub().get_all_level_actors():
        if isinstance(a, (unreal.LandscapeProxy, unreal.DecalActor)):
            continue
        label, cls = a.get_actor_label(), a.get_class().get_name()
        try:
            folder = str(a.get_folder_path())
        except Exception:  # noqa
            folder = ''
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.static_mesh
            if sm is None or not c.is_visible() or c.get_editor_property('hidden_in_game'):
                continue
            path = sm.get_path_name()
            meshes.setdefault(path, len(meshes))
            base = {'a': label, 'cls': cls, 'folder': folder, 'c': c.get_name(), 'm': meshes[path],
                    'col': c.get_collision_enabled().name}
            if isinstance(c, unreal.InstancedStaticMeshComponent):
                n = c.get_instance_count()
                base['inst'] = [_xf(c.get_instance_transform(i, True)) for i in range(n)]
            else:
                base['xf'] = _xf(c.get_world_transform())
            rows.append(base)
    with open(OUT + 'components.json', 'w') as f:
        json.dump({'meshes': sorted(meshes, key=meshes.get), 'rows': rows}, f)
    print('components %d, unique meshes %d, instances %d' % (
        len(rows), len(meshes), sum(len(r.get('inst', [0])) for r in rows)))


def dump_meshes():
    data = json.load(open(OUT + 'components.json'))
    done = failed = 0
    for index, path in enumerate(data['meshes']):
        target = OUT + 'mesh/%d.bin' % index
        if os.path.exists(target):
            done += 1
            continue
        sm = unreal.load_asset(path.split('.')[0])
        verts, tris = array.array('f'), array.array('i')
        try:
            for section in range(sm.get_num_sections(0)):
                v, t, _n, _uv, _tan = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, section)
                off = len(verts) // 3
                for p in v:
                    verts.extend((p.x, p.y, p.z))
                tris.extend(i + off for i in t)
        except Exception as error:  # noqa
            print('FAILED', path, error)
            failed += 1
            continue
        with open(target, 'wb') as f:
            array.array('i', [len(verts) // 3, len(tris) // 3]).tofile(f)
            verts.tofile(f)
            tris.tofile(f)
        done += 1
    print('meshes written %d, failed %d' % (done, failed))
