# Shared-editor probe (PROTOTYPE debugging): captures the DeployProbe_* actors spawned far from the play area.
# Expects TAG (file name suffix). Progress is set in a separate call beforehand so an editor tick sends it to the GPU.
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/DeployAnimProto/probe'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
found = {a.get_actor_label(): a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('DeployProbe_')}
cam = found['DeployProbe_cam']
comp = cam.capture_component2d
if not comp.texture_target:
    comp.texture_target = unreal.RenderingLibrary.create_render_target2d(w, 1280, 800, unreal.TextureRenderTargetFormat.RTF_RGBA8)
comp.fov_angle = 40.0
comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 0')
try:
    for key in ('wpo', 'vat'):
        a = found['DeployProbe_' + key]
        target = a.get_actor_location() + unreal.Vector(-148, -49, 260)
        eye = target + unreal.Vector(2350, -1700, 2700)
        cam.set_actor_location(eye, False, False)
        cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), False)
        comp.capture_scene()
        unreal.RenderingLibrary.export_render_target(w, comp.texture_target, OUT, '%s_%s.png' % (key, TAG))
finally:
    unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 1')
print('captured', TAG)
