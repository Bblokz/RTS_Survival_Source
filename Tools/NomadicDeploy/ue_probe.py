# Unreal (editor python): quick look at a deploy mesh at a scrubbed progress, far from the play area.
# Globals: ACTION = spawn | set | capture | cleanup; MESH (spawn), PROGRESS (set), TAG (capture), OUT (capture dir).
# Progress is written in a separate call from the capture so an editor tick sends it to the GPU.
import unreal

SPOT = unreal.Vector(0, 420000, 20000)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
found = {a.get_actor_label(): a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('NomadicProbe_')}

if ACTION == 'spawn':
    for a in found.values():
        actors.destroy_actor(a)
    plane = actors.spawn_actor_from_class(unreal.StaticMeshActor, SPOT)
    plane.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    plane.set_actor_scale3d(unreal.Vector(400, 400, 1))
    plane.set_actor_label('NomadicProbe_ground')
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, SPOT, unreal.Rotator(roll=0, pitch=-50, yaw=150))
    sun.light_component.set_intensity(6)
    sun.set_actor_label('NomadicProbe_sun')
    sky = actors.spawn_actor_from_class(unreal.SkyLight, SPOT)
    sky.light_component.set_intensity(1.5)
    sky.light_component.recapture_sky()
    sky.set_actor_label('NomadicProbe_sky')
    m = actors.spawn_actor_from_class(unreal.StaticMeshActor, SPOT)
    m.set_actor_label('NomadicProbe_mesh')
    c = m.static_mesh_component
    c.set_static_mesh(unreal.load_asset(MESH))
    c.set_editor_property('bounds_scale', 3.0)
    cam = actors.spawn_actor_from_class(unreal.SceneCapture2D, SPOT)
    cam.set_actor_label('NomadicProbe_cam')
    print('spawned', MESH, 'bounds', c.static_mesh.get_bounds().box_extent)
elif ACTION == 'set':
    found['NomadicProbe_mesh'].static_mesh_component.set_custom_primitive_data_float(3, float(PROGRESS))
    print('progress', PROGRESS)
elif ACTION == 'capture':
    cam = found['NomadicProbe_cam']
    mesh = found['NomadicProbe_mesh']
    comp = cam.capture_component2d
    if not comp.texture_target:
        comp.texture_target = unreal.RenderingLibrary.create_render_target2d(world, 1280, 800,
                                                                             unreal.TextureRenderTargetFormat.RTF_RGBA8)
    comp.fov_angle = 40.0
    comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    ext = mesh.static_mesh_component.static_mesh.get_bounds().box_extent
    radius = max(ext.x, ext.y, ext.z)
    target = mesh.get_actor_location() + unreal.Vector(0, 0, ext.z * 0.5)
    eye = target + unreal.Vector(1.45, -1.05, 1.7) * radius * 1.55
    cam.set_actor_location(eye, False, False)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), False)
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    try:
        comp.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, comp.texture_target, OUT, TAG + '.png')
    finally:
        unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 1')
    print('captured', OUT + '/' + TAG + '.png')
elif ACTION == 'cleanup':
    for a in found.values():
        actors.destroy_actor(a)
    print('cleaned', len(found))
