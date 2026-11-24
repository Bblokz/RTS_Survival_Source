"""Job 1: import the Panzerwerfer textures, meshes and track anims, make the MIs, sockets, collision and physics asset.
Run: python u.py py pw_common.py pw_import.py   (re-runnable; replaces its own assets only)."""
assert_no_pie()
before = dirty_names()
print('DIRTY BEFORE', before)

# ---------------- textures ----------------
TEX = {
    'T_PanzerWerfer_Hull_D': ('T_PanzerWerfer_Hull_D.png', False),
    'T_PanzerWerfer_FrontWheel_D': ('T_PanzerWerfer_FrontWheel_D.png', False),
    'T_PanzerWerfer_FrontWheel_N': ('T_PanzerWerfer_FrontWheel_N.png', True),
    'T_PanzerWerfer_TracksWheels_D': ('T_PanzerWerfer_TracksWheels_D.png', False),
    'T_PanzerWerfer_TracksWheels_N': ('T_PanzerWerfer_TracksWheels_N.png', True),
    'T_PanzerWerfer_MG42_D': ('T_PanzerWerfer_MG42_D.png', False),
    'T_PanzerWerfer_MG42_N': ('T_PanzerWerfer_MG42_N.png', True),
}
for name, (png, normal) in TEX.items():
    import_texture(png, name, normal)
T = lambda n: TEXDIR + '/' + n

# ---------------- materials ----------------
jp = load(JP_HULL_MI)
jp_scalars = {s.parameter_info.name: s.parameter_value for s in jp.get_editor_property('scalar_parameter_values')}
make_mi('MI_PanzerWerfer_Hull', FULLUV_MASTER,
        {'TankDiffuse': T('T_PanzerWerfer_Hull_D'), 'TankNormal': FLAT_NORMAL}, jp_scalars)
make_mi('MI_PanzerWerfer_Wheels', MM_WHEELS,
        {'Diffuse': T('T_PanzerWerfer_TracksWheels_D'), 'Normal': T('T_PanzerWerfer_TracksWheels_N')})
make_mi('MI_PanzerWerfer_Tracks', MM_TRACKS,
        {'Diffuse': T('T_PanzerWerfer_TracksWheels_D'), 'Normal': T('T_PanzerWerfer_TracksWheels_N')})
make_mi('MI_PanzerWerfer_FrontWheel', MM_WHEELS,
        {'Diffuse': T('T_PanzerWerfer_FrontWheel_D'), 'Normal': T('T_PanzerWerfer_FrontWheel_N')})
make_mi('MI_PanzerWerfer_MG42', MM_ENV,
        {'Diffuse': T('T_PanzerWerfer_MG42_D'), 'NormalMap': T('T_PanzerWerfer_MG42_N')})
# Wreck: the Keubel/BM-13 burn shader over the live textures.
make_mi('MI_PanzerWerfer_Hull_Destroyed', BURNT_MASTER,
        {'DiffuseMap': T('T_PanzerWerfer_Hull_D'), 'NormalMap': FLAT_NORMAL}, {'BurnBrightness': 0.3})
make_mi('MI_PanzerWerfer_Wheels_Destroyed', BURNT_MASTER,
        {'DiffuseMap': T('T_PanzerWerfer_TracksWheels_D'), 'NormalMap': T('T_PanzerWerfer_TracksWheels_N')},
        {'BurnBrightness': 0.3})
make_mi('MI_PanzerWerfer_FrontWheel_Destroyed', BURNT_MASTER,
        {'DiffuseMap': T('T_PanzerWerfer_FrontWheel_D'), 'NormalMap': T('T_PanzerWerfer_FrontWheel_N')},
        {'BurnBrightness': 0.3})
make_mi('MI_PanzerWerfer_MG42_Destroyed', BURNT_MASTER,
        {'DiffuseMap': T('T_PanzerWerfer_MG42_D'), 'NormalMap': T('T_PanzerWerfer_MG42_N')}, {'BurnBrightness': 0.3})
make_mi('MI_PanzerWerfer_Launcher_Destroyed', BURNT_MASTER,
        {'DiffuseMap': LAUNCHER_D, 'NormalMap': LAUNCHER_N}, {'BurnBrightness': 0.3})
M = lambda n: MATS + '/' + n

# ---------------- static meshes ----------------
for fbx, name in (('SM_PanzerWerfer_Hull.fbx', 'SM_PanzerWerfer_Hull'),
                  ('SM_Collision_PanzerWerfer.fbx', 'SM_Collision_PanzerWerfer'),
                  ('SM_PanzerWerfer_HullMg.fbx', 'SM_PanzerWerfer_HullMg'),
                  ('SM_Destroyed_PanzerWerfer.fbx', 'SM_Destroyed_PanzerWerfer')):
    AT.import_asset_tasks([fbx_task(EXP + fbx, MESHES, name, static_ui())])
AT.import_asset_tasks([fbx_task(EXP + 'SM_PanzerWerfer_TrainingPreview.fbx', PW, 'SM_PanzerWerfer_TrainingPreview',
                                static_ui())])

hull = load(MESHES + '/SM_PanzerWerfer_Hull')
set_static_slots(hull, {'M_PanzerWerfer_Hull': M('MI_PanzerWerfer_Hull')})
clear_static_sockets(hull)
for sname, loc in HULL_SOCKETS.items():
    add_static_socket(hull, sname, loc)
col = load(MESHES + '/SM_Collision_PanzerWerfer')
set_static_slots(col, {'M_PanzerWerfer_Hull': '/Engine/EngineMaterials/WorldGridMaterial'})
hull.set_editor_property('complex_collision_mesh', col)
bs = hull.get_editor_property('body_setup')
bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)

mg = load(MESHES + '/SM_PanzerWerfer_HullMg')
set_static_slots(mg, {'M_PanzerWerfer_MG42': M('MI_PanzerWerfer_MG42')})
clear_static_sockets(mg)
add_static_socket(mg, 'BulletLaunch0', MG_MUZZLE)

wreck = load(MESHES + '/SM_Destroyed_PanzerWerfer')
set_static_slots(wreck, {'M_PanzerWerfer_Hull': M('MI_PanzerWerfer_Hull_Destroyed'),
                         'M_PanzerWerfer_MG42': M('MI_PanzerWerfer_MG42_Destroyed'),
                         'M_PanzerWerfer_Wheels': M('MI_PanzerWerfer_Wheels_Destroyed'),
                         'M_PanzerWerfer_FrontWheel': M('MI_PanzerWerfer_FrontWheel_Destroyed'),
                         'M_PanzerWerfer_Launcher': M('MI_PanzerWerfer_Launcher_Destroyed')})
clear_static_sockets(wreck)
add_static_socket(wreck, 'Exp1', WRECK_EXP1)
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
sms.remove_collisions(wreck)
sms.add_simple_collisions(wreck, unreal.ScriptCollisionShapeType.BOX)
wreck.get_editor_property('body_setup').set_editor_property('collision_trace_flag',
                                                            unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)

prev = load(PW + '/SM_PanzerWerfer_TrainingPreview')
set_static_slots(prev, {'MI_WhileConstructing': WHILE_CONSTRUCTING})

for m in (hull, col, mg, wreck, prev):
    nuv = sms.get_num_uv_channels(m, 0)
    if m.get_editor_property('light_map_coordinate_index') > nuv - 1:
        m.set_editor_property('light_map_coordinate_index', nuv - 1)
    save(m)
    b = m.get_bounding_box()
    print('SM', m.get_name(), 'tris', m.get_num_triangles(0), 'bounds', b.min, b.max,
          'slots', [(str(s.get_editor_property('material_slot_name')),
                     s.get_editor_property('material_interface').get_name()
                     if s.get_editor_property('material_interface') else None)
                    for s in m.get_editor_property('static_materials')],
          'sockets', [str(s.get_editor_property('socket_name')) for s in m.get_sockets_by_tag('')])

print('DIRTY AFTER', [d for d in dirty_names() if d not in before])
print('IMPORT STATIC DONE')
