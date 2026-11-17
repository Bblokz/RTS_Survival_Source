# Table of every LAND_* actor (and what stays on/near the slab) in slab-local coordinates, for reviewers.
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/landing/landing_lib.py').read())
rows = ['label | class / mesh | u | v | z | yaw(local) | scale']
for a in sorted(all_actors(), key=lambda a: a.get_actor_label()):
    lab = a.get_actor_label()
    p = a.get_actor_location()
    u, v = L(p.x, p.y)
    near = abs(u) < 6500 and abs(v) < 5500
    if not (lab.startswith('LAND_') or (near and lab.startswith('REAR_'))):
        continue
    what = a.get_class().get_name()
    if isinstance(a, unreal.StaticMeshActor) and a.static_mesh_component.static_mesh:
        what += ' / ' + a.static_mesh_component.static_mesh.get_name()
    s = a.get_actor_scale3d()
    rows.append('%s | %s | %.0f | %.0f | %.0f | %.0f | %.2f,%.2f,%.2f' % (lab, what, u, v, p.z, a.get_actor_rotation().yaw - YAW, s.x, s.y, s.z))
open(ROOT + 'actor_table.txt', 'w').write('\n'.join(rows))
print(len(rows) - 1, 'rows')
