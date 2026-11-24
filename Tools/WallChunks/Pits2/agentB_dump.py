"""Agent B: read-only dump of the two-mesh B-4 / ML-20 pit meshes: slots, bounds, render triangles (collision_gap.py
tris.bin format, slot = section index) and the collision description backup. Run in the shared editor (reads only)."""
import struct
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'
D = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes/'
MESHES = {
    'B4Pit_Sandbags': D + 'SM_B4Pit_Sandbags',
    'B4Pit_Emplacement': D + 'SM_B4Pit_Emplacement',
    'B4ConcretePit_Sandbags': D + 'SM_B4ConcretePit_Sandbags',
    'B4ConcretePit': D + 'SM_B4ConcretePit',
}
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for key, path in MESHES.items():
    sm = unreal.load_asset(path)
    records = bytearray()
    for sec in range(sm.get_num_sections(0)):
        verts, tris, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, sec)
        for t in range(0, len(tris), 3):
            p = [verts[tris[t + k]] for k in range(3)]
            records += struct.pack('<h9f', sec, p[0].x, p[0].y, p[0].z, p[1].x, p[1].y, p[1].z, p[2].x, p[2].y, p[2].z)
        print(key, 'section', sec, 'tris', len(tris) // 3)
    with open(ROOT + 'agentB_' + key + '_tris.bin', 'wb') as handle:
        handle.write(bytes(records))
    body = sm.get_editor_property('body_setup')
    agg = body.get_editor_property('agg_geom')
    lines = ['flag %s' % body.get_editor_property('collision_trace_flag')]
    lines.append('convex_elems %d' % len(agg.get_editor_property('convex_elems')))
    try:
        lines.append('agg_geom ' + agg.export_text())
    except Exception as error:  # noqa: BLE001
        lines.append('agg_geom export failed %s' % error)
    for kind in ('box_elems', 'sphere_elems', 'sphyl_elems'):
        lines.append('%s %d' % (kind, len(agg.get_editor_property(kind))))
    with open(ROOT + 'agentB_' + key + '_collision_backup.txt', 'w', encoding='utf-8') as handle:
        handle.write(chr(10).join(lines))
