"""Job 1c: re-import the hull-only collision (build_collision.py) over SM_Collision_PanzerWerfer, keep it as the hull's
complex collision. Run: python pw_uecp.py py pw_common.py pw_reimport_collision.py"""
assert_no_pie()
before = dirty_names()
AT.import_asset_tasks([fbx_task(EXP + 'SM_Collision_PanzerWerfer.fbx', MESHES, 'SM_Collision_PanzerWerfer', static_ui())])
col = load(MESHES + '/SM_Collision_PanzerWerfer')
set_static_slots(col, {'M_PanzerWerfer_Hull': '/Engine/EngineMaterials/WorldGridMaterial'})
hull = load(MESHES + '/SM_PanzerWerfer_Hull')
hull.set_editor_property('complex_collision_mesh', col)
hull.get_editor_property('body_setup').set_editor_property('collision_trace_flag',
                                                           unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
save(col)
save(hull)
b = col.get_bounding_box()
print('collision', b.min, b.max, 'tris', col.get_num_triangles(0), 'hull complex', hull.get_editor_property('complex_collision_mesh').get_name())
print('DIRTY AFTER', [d for d in dirty_names() if d not in before])
