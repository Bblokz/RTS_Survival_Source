# Cuts the sandbags that stand in the Strike Division HQ's unload bay R out of HQ_BuildingMesh (done 2026-11-24).
# Run headless: UnrealEditor-Win64-DebugGame-Cmd <uproject> -run=pythonscript -script=<this> -nullrhi -unattended
# Method: delete the sandbag polygons through the mesh description on a scratch copy A (fast build, no welding), then
# copy A's LOD 0 into a fresh copy B and the real mesh with set_lod_from_static_mesh, so they rebuild with the mesh's own
# build settings. An FBX round trip (export, Blender, reimport) halved the render vertices (the mesh was merged in UE, not
# imported), and build_from_static_mesh_descriptions alone does not weld (3.8x the vertices). Guards: vertex ratio 0.95..1,
# same sockets, UV channels, collision complexity, Nanite and slots, else nothing is saved.
# The save fails while any -game run has the HQ loaded; wait for it.

# First on a duplicate; only if the duplicate keeps everything but the cut is the real mesh edited and saved. Prints HQD.
import unreal

MESH = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/BuildingMeshes/HQ_BuildingMesh'
SCRATCH = '/Game/_ScratchHQDesc/HQ_DescCut'
SANDBAG_MATERIAL = 'M_GerSandBagScaffold_Atlas'
BAY_X, BAY_Y, BAY_MAX_Z = (495.0, 1120.0), (100.0, 424.0), 160.0
MIN_ISLAND_TOP_Z = 16.0  # flatter islands are the emplacement's floor boards: keep them
EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def snapshot(mesh):
    return {'verts': SMS.get_number_verts(mesh, 0), 'uv': SMS.get_num_uv_channels(mesh, 0),
            'sockets': len(mesh.get_sockets_by_tag('')), 'complexity': str(SMS.get_collision_complexity(mesh)),
            'nanite': mesh.get_editor_property('nanite_settings').get_editor_property('enabled'),
            'slots': [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]}


def sandbag_group_ids(mesh, desc):
    # Polygon groups carry no slot name in python. Only the sandbags rise above the floor boards inside bay R (checked in
    # Blender), so the one group with such geometry there is the sandbag group; more than one means stop.
    found = []
    for g in range(desc.get_polygon_group_count()):
        hits = 0
        for p in desc.get_polygon_group_polygons(unreal.PolygonGroupID(g)):
            pts = [desc.get_vertex_position(desc.get_vertex_instance_vertex(vi))
                   for vi in desc.get_polygon_vertex_instances(p)]
            if all(BAY_X[0] < c.x < BAY_X[1] and BAY_Y[0] < c.y < BAY_Y[1] for c in pts) and max(c.z for c in pts) > MIN_ISLAND_TOP_Z:
                hits += 1
        if hits:
            found.append((g, hits))
    print('HQD groups with raised geometry in bay R', found)
    return [g for g, _ in found] if len(found) == 1 else []


def cut(mesh):
    desc = mesh.get_static_mesh_description(0)
    groups = sandbag_group_ids(mesh, desc)
    if not groups:
        return 0
    print('HQD sandbag polygon groups', groups, 'of', desc.get_polygon_group_count())
    polys = []
    for g in groups:
        polys += [p.id_value for p in desc.get_polygon_group_polygons(unreal.PolygonGroupID(g))]
    poly_verts, vert_polys, pos = {}, {}, {}
    for p in polys:
        vids = []
        for vi in desc.get_polygon_vertex_instances(unreal.PolygonID(p)):
            v = desc.get_vertex_instance_vertex(vi).id_value
            vids.append(v)
            vert_polys.setdefault(v, []).append(p)
            if v not in pos:
                c = desc.get_vertex_position(unreal.VertexID(v))
                pos[v] = (c.x, c.y, c.z)
        poly_verts[p] = vids
    seen, doomed, kept_flat = set(), [], 0
    for p in polys:
        if p in seen:
            continue
        stack, island = [p], []
        seen.add(p)
        while stack:
            q = stack.pop()
            island.append(q)
            for v in poly_verts[q]:
                for r in vert_polys[v]:
                    if r not in seen:
                        seen.add(r)
                        stack.append(r)
        pts = [pos[v] for q in island for v in poly_verts[q]]
        if not any(BAY_X[0] < x < BAY_X[1] and BAY_Y[0] < y < BAY_Y[1] and z < BAY_MAX_Z for x, y, z in pts):
            continue
        if max(z for _, _, z in pts) < MIN_ISLAND_TOP_Z:
            kept_flat += 1
            continue
        doomed += island
    print('HQD sandbag polygons', len(polys), 'deleting', len(doomed), 'kept flat islands in bay', kept_flat)
    for p in doomed:
        desc.delete_polygon(unreal.PolygonID(p))
    mesh.build_from_static_mesh_descriptions([desc])
    return len(doomed)


original = unreal.load_asset(MESH)
before = snapshot(original)
print('HQD before', before)
EAL.delete_directory('/Game/_ScratchHQDesc')
# A: the cut, fast-built from its description (no welding). B: a fresh copy that takes A's LOD 0 and rebuilds it with the
# original's own build settings, the way the real mesh will.
EAL.duplicate_asset(MESH, SCRATCH)
EAL.duplicate_asset(MESH, SCRATCH + '_B')
scratch = unreal.load_asset(SCRATCH)
deleted = cut(scratch)
target = unreal.load_asset(SCRATCH + '_B')
print('HQD set_lod scratch', SMS.set_lod_from_static_mesh(target, 0, scratch, 0, True))
s = snapshot(target)
ratio = s['verts'] / before['verts']
ok = (deleted > 0 and 0.95 < ratio < 1.0 and s['uv'] == before['uv'] and s['sockets'] == before['sockets']
      and s['complexity'] == before['complexity'] and s['nanite'] == before['nanite'] and s['slots'] == before['slots'])
print('HQD scratch B after', {k: s[k] for k in ('verts', 'uv', 'sockets', 'complexity', 'nanite')}, 'ratio %.4f ok %s' % (ratio, ok))
if ok:
    print('HQD set_lod real', SMS.set_lod_from_static_mesh(original, 0, scratch, 0, True))
    a = snapshot(original)
    ratio = a['verts'] / before['verts']
    final_ok = (0.95 < ratio < 1.0 and a['uv'] == before['uv'] and a['sockets'] == before['sockets']
                and a['complexity'] == before['complexity'] and a['nanite'] == before['nanite'] and a['slots'] == before['slots'])
    print('HQD real after', {k: a[k] for k in ('verts', 'uv', 'sockets', 'complexity', 'nanite')}, 'ratio %.4f ok %s' % (ratio, final_ok))
    if final_ok:
        print('HQD saved', EAL.save_asset(MESH, only_if_is_dirty=False))
EAL.delete_directory('/Game/_ScratchHQDesc')
