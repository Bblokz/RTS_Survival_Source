"""Void trace grid for the skirt planner (exec in a throwaway editor with the map open, SKIRT_NAME set): traces a 10 m grid
over the camera-visible area (fog-of-war square + 45 m forward view) and lists water planes -> void.json."""
import json
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
asub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
OUT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/%s/' % SKIRT_NAME   # set SKIRT_NAME before exec
fow = [a for a in asub.get_all_level_actors() if a.get_class().get_name().startswith('BP_FogOfWar_Manager')][0]
fx, fy, ext = fow.get_actor_location().x, fow.get_actor_location().y, fow.get_editor_property('map_extent')
x0, x1 = fx - ext - 1500, fx + ext + 4500     # camera looks towards +X: ~45 m ahead of the clamp
y0, y1 = fy - ext - 2500, fy + ext + 2500
step = 1000.0
res = {'x0': x0, 'y0': y0, 'step': step, 'rows': []}
nx, ny = int((x1 - x0) / step) + 1, int((y1 - y0) / step) + 1
for i in range(nx):
    row = []
    for j in range(ny):
        x, y = x0 + i * step, y0 + j * step
        hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            row.append('V')
        else:
            t = hit.to_tuple()
            a = t[9]
            row.append('L' if a and a.get_class().get_name() == 'Landscape' else 'M')
    res['rows'].append(''.join(row))
waters = []
for a in asub.get_all_level_actors():
    if isinstance(a, unreal.StaticMeshActor) and a.static_mesh_component.static_mesh:
        m = a.static_mesh_component.get_material(0)
        if m and 'Water' in m.get_name():
            o, e = a.get_actor_bounds(False)
            waters.append([a.get_actor_label(), m.get_name(), o.x, o.y, o.z, e.x, e.y, a.static_mesh_component.get_collision_enabled().name])
res['waters'] = waters
json.dump(res, open(OUT + 'void.json', 'w'))
print('grid', nx, ny, 'void cells', sum(r.count('V') for r in res['rows']), 'waters', waters)
