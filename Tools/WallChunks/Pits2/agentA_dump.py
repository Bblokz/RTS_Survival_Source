"""Agent A: read-only dump of the 5 pit meshes: slots, bounds, render triangles (collision_gap.py tris.bin format,
slot = section material index) and the collision description backup. Run in the shared editor (reads only)."""
import struct
import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/'
MESHES = {
    'TwinMG42': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/TwinMG42Pit/Meshes/SM_GerPit_TwinMG42_Sandbags',
    'BS3': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Meshes/SM_BS3Pit',
    'Mortar120': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusMortarPit/Meshes/SM_Rus120MortarPit',
    'B4Stepped': '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes/SM_B4Pit_SteppedSquare',
    'ML20Twin': '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Meshes/SM_ML20Pit_TwinShoulders',
}
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for key, path in MESHES.items():
    if 'ONLY' in globals() and key not in ONLY:
        continue
    sm = unreal.load_asset(path)
    b = sm.get_bounding_box()
    print('==', key, sm.get_name(), 'bounds (%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)' % (b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z),
          'nanite', sm.get_editor_property('nanite_settings').enabled)
    for i, s in enumerate(sm.static_materials):
        print('   slot', i, s.material_slot_name, s.material_interface.get_path_name() if s.material_interface else None)
    records = bytearray()
    sections = sm.get_num_sections(0)
    for sec in range(sections):
        verts, tris, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, sec)
        slot = sm.get_material_index(sm.get_material_slot_names()[0]) if False else sec
        info = sms.get_lod_material_slot(sm, 0, sec) if hasattr(sms, 'get_lod_material_slot') else sec
        for t in range(0, len(tris), 3):
            p = [verts[tris[t + k]] for k in range(3)]
            records += struct.pack('<h9f', info, p[0].x, p[0].y, p[0].z, p[1].x, p[1].y, p[1].z, p[2].x, p[2].y, p[2].z)
        print('   section', sec, 'slot', info, 'tris', len(tris) // 3)
    with open(ROOT + 'agentA_' + key + '_tris.bin', 'wb') as handle:
        handle.write(bytes(records))
    body = sm.get_editor_property('body_setup')
    agg = body.get_editor_property('agg_geom')
    with open(ROOT + 'agentA_' + sm.get_name() + '_collision_backup.txt', 'w', encoding='utf-8') as handle:
        handle.write('flag %s%sconvex_elems %d%sagg_geom %s' % (body.get_editor_property('collision_trace_flag'), chr(10),
                     len(agg.get_editor_property('convex_elems')), chr(10), agg.export_text()))
    print('   flag', body.get_editor_property('collision_trace_flag'), 'convex', sms.get_convex_collision_count(sm),
          'simple', sms.get_simple_collision_count(sm) if hasattr(sms, 'get_simple_collision_count') else '?')
