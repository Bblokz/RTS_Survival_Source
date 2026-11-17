"""Checks SM_LandingPad_Soviet and its materials, then captures the four pads on six SM_Ground_1 apron tiles (the real
72 x 48 m apron) far below the level (z -100000): a top-down ortho shot and a 3/4 shot into
Saved/IntroLanding/pads/. Every spawned actor is destroyed in `finally`; nothing in the level is saved.

Run:  python Source/Tools/IntroMission/landing/landing_cmd.py Source/Tools/IntroMission/landing/landing_pad_verify.py
"""
import os

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/IntroMission/LandingField'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/IntroLanding/pads'
Z0 = -100000.0
SME = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
ACT = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MEL = unreal.MaterialEditingLibrary

if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
    raise RuntimeError('PIE is running')

mesh = unreal.load_asset(f'{HOME}/SM_LandingPad_Soviet')
bb = mesh.get_bounding_box()
print('bounds min', bb.min, 'max', bb.max)
print('size cm', bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z)
print('nanite', mesh.get_editor_property('nanite_settings').get_editor_property('enabled'))
for lod in range(mesh.get_num_lods()):
    print('LOD', lod, 'verts', SME.get_number_verts(mesh, lod), 'tris', mesh.get_num_triangles(lod))
body = mesh.get_editor_property('body_setup')
agg = body.get_editor_property('agg_geom')
print('simple collision: convex', len(agg.get_editor_property('convex_elems')), 'box', len(agg.get_editor_property('box_elems')),
      'sphyl', len(agg.get_editor_property('sphyl_elems')), 'sphere', len(agg.get_editor_property('sphere_elems')),
      'trace flag', body.get_editor_property('collision_trace_flag'))
print('convex collision count (SME)', SME.get_convex_collision_count(mesh), 'shape count', SME.get_simple_collision_count(mesh))

for i, s in enumerate(mesh.get_editor_property('static_materials')):
    print('slot', i, s.get_editor_property('material_slot_name'), s.get_editor_property('material_interface').get_path_name())
for n in ('1', '2', '3', '4', 'Glass'):
    mi = unreal.load_asset(f'{HOME}/MI_LandingPad_Soviet_{n}')
    print('MI', n, 'exists' if mi else 'MISSING',
          'NumberIndex', MEL.get_material_instance_scalar_parameter_value(mi, 'NumberIndex'),
          'IsGlass', MEL.get_material_instance_static_switch_parameter_value(mi, 'IsGlass'),
          'LightIntensity', MEL.get_material_instance_scalar_parameter_value(mi, 'LightIntensity'))

ground = unreal.load_asset('/Game/RTS_Survival/Meshes/PresetRegion_PlayerStart/AirfieldGround/SM_Ground_1')
spawned = []
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(None, 'r.TextureStreaming 0')
try:
    for ox in (-1200, 1200, 3600):          # SM_Ground_1 spans -2400..0 from its pivot
        for oy in (0, 2400):
            spawned.append(ACT.spawn_actor_from_object(ground, unreal.Vector(ox, oy, Z0), unreal.Rotator(0, 0, 0)))
    for k, (x, y) in enumerate(((1800, -1200), (1800, 1200), (-1800, -1200), (-1800, 1200))):
        a = ACT.spawn_actor_from_object(mesh, unreal.Vector(x, y, Z0 - 1.5), unreal.Rotator(0, 0, 0))
        a.static_mesh_component.set_material(0, unreal.load_asset(f'{HOME}/MI_LandingPad_Soviet_{k + 1}'))
        spawned.append(a)
    # collision probe on pad 1 (pivot at Z0 - 1.5): centre, ramp middle, just outside
    px, py, pz = 1800.0, -1200.0, Z0 - 1.5
    for label, dx in (('centre', 0.0), ('top edge', 1050 - 70.0), ('ramp mid', 1050 - 32.0), ('toe', 1050 - 1.0), ('outside', 1060.0)):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(px + dx, py, pz + 500), unreal.Vector(px + dx, py, pz - 50),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, spawned[:6], unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            print('trace', label, 'miss')
            continue
        t = hit.to_tuple()
        print('trace', label, 'hit' if t[0] else 'miss', 'z above pad bottom %.1f cm' % (t[4].z - pz) if t[0] else '',
              t[9].get_name() if t[0] and t[9] else '')
    cap = ACT.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, Z0 + 10000), unreal.Rotator(roll=0, pitch=-90, yaw=0))
    spawned.append(cap)
    comp = cap.capture_component2d
    comp.set_editor_property('capture_every_frame', False)
    comp.set_editor_property('capture_on_movement', False)
    comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property('show_flag_settings', [unreal.EngineShowFlagsSetting(show_flag_name=n, enabled=False)
                                                    for n in ('Fog', 'VolumetricFog', 'Atmosphere', 'Cloud')])
    pp = comp.get_editor_property('post_process_settings')     # the map's lighting is dim: lift the capture only
    pp.set_editor_property('override_auto_exposure_bias', True)
    pp.set_editor_property('auto_exposure_bias', float(os.environ.get('PAD_EV', '4.5')))
    comp.set_editor_property('post_process_settings', pp)
    shots = [
        ('ue_top', unreal.Vector(0, 0, Z0 + 10000), unreal.Rotator(roll=0, pitch=-90, yaw=0), True, 1400, 2000),
        ('ue_iso', unreal.Vector(-5200, -3800, Z0 + 4600), unreal.Rotator(roll=0, pitch=-42, yaw=36), False, 1800, 1100),
        ('ue_near', unreal.Vector(-200, -2600, Z0 + 1500), unreal.Rotator(roll=0, pitch=-38, yaw=26), False, 1800, 1100),
    ]
    for name, loc, rot, ortho, w, h in shots:
        rt = unreal.RenderingLibrary.create_render_target2d(world, w, h, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        comp.set_editor_property('texture_target', rt)
        comp.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC if ortho else unreal.CameraProjectionMode.PERSPECTIVE)
        comp.set_editor_property('ortho_width', 5200.0)
        comp.set_editor_property('fov_angle', 50.0)
        cap.set_actor_location_and_rotation(loc, rot, False, False)
        unreal.SystemLibrary.execute_console_command(world, 'FlushRenderingCommands')
        for source, tag in ((unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR, ''), (unreal.SceneCaptureSource.SCS_BASE_COLOR, '_basecolor')):
            comp.set_editor_property('capture_source', source)
            comp.capture_scene()
            unreal.RenderingLibrary.export_render_target(world, rt, OUT, name + tag + '.png')
            print('captured', OUT + '/' + name + tag + '.png')
finally:
    for a in spawned:
        a.destroy_actor()
    unreal.SystemLibrary.execute_console_command(None, 'r.TextureStreaming 1')
    stray = [a.get_actor_label() for a in ACT.get_all_level_actors() if a.get_actor_location().z < -50000]
    print('stray actors below -50000:', stray)
