"""Read-only dump of Ger_Berlin for the rubble decal generator (throwaway editor, nothing saved).

Writes Saved/Berlin/rubble_decals/level_dump.json:
  actors: [label, class, folder, x, y, z, yaw, mesh, local bbox min/max (cm, unscaled), scale]
for every actor except plain lights/volumes; static mesh bounds come from the first StaticMeshComponent's mesh asset
(local space), so footprints follow the piece's own axes (x = street front, see footprint_table.json).
"""
import json
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/level_dump.json'
S = {'t': 0.0, 'done': False}


def dump():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    rows = []
    mesh_box = {}
    for a in sub.get_all_level_actors():
        cls = a.get_class().get_name()
        if cls in ('WorldDataLayers', 'WorldPartitionMiniMap', 'Brush'):
            continue
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        row = {'label': a.get_actor_label(), 'class': cls, 'folder': str(a.get_folder_path()),
               'x': round(loc.x, 1), 'y': round(loc.y, 1), 'z': round(loc.z, 1), 'yaw': round(rot.yaw, 2),
               'scale': round(a.get_actor_scale3d().x, 3)}
        if cls == 'DecalActor':
            d = a.decal
            m = d.get_decal_material()
            row['mat'] = m.get_name() if m else None
            s = d.get_editor_property('decal_size')
            row['size'] = [round(s.x, 1), round(s.y, 1), round(s.z, 1)]
        else:
            comps = a.get_components_by_class(unreal.StaticMeshComponent)
            for c in comps:
                if isinstance(c, unreal.InstancedStaticMeshComponent):
                    continue
                sm = c.static_mesh
                if sm is None:
                    continue
                key = sm.get_path_name()
                if key not in mesh_box:
                    b = sm.get_bounding_box()
                    mesh_box[key] = [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1),
                                     round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)]
                row['mesh'] = key.split('.')[-1]
                row['box'] = mesh_box[key]
                rel = c.get_editor_property('relative_location')
                row['mesh_rel'] = [round(rel.x, 1), round(rel.y, 1), round(c.get_editor_property('relative_rotation').yaw, 2)]
                break
            ism = a.get_components_by_class(unreal.InstancedStaticMeshComponent)
            if ism and a.get_actor_label().startswith(('FOOT', 'BER_Rubble', 'RUBBLE')):
                inst = []
                for c in ism:
                    if c.static_mesh is None:
                        continue
                    nm = c.static_mesh.get_name()
                    for i in range(c.get_instance_count()):
                        tr = c.get_instance_transform(i, True)
                        inst.append([nm, round(tr.translation.x, 1), round(tr.translation.y, 1),
                                     round(tr.rotation.rotator().yaw, 1), round(tr.scale3d.x, 2)])
                row['instances'] = inst
        rows.append(row)
    json.dump({'map': world.get_path_name(), 'actors': rows}, open(OUT, 'w'), indent=0)
    unreal.log('RUBDUMP wrote %d actors' % len(rows))


def tick(delta):
    S['t'] += delta
    if S['done']:
        return
    if S['t'] > 20.0:
        S['done'] = True
        try:
            dump()
        except Exception as e:  # noqa: BLE001
            unreal.log('RUBDUMP ERROR %r' % e)
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
unreal.log('RUBDUMP loaded')
