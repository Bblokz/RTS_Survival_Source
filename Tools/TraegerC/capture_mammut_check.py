"""One capture of the Mammut glider, to prove a change to the shared M_Mammut_Airframe left it alone (run in the
editor). Set MAMMUT_SHOT to the file name first; writes Saved/TraegerC/<MAMMUT_SHOT>.png.
"""
import unreal

MESH = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut/Meshes/SM_Mammut_Body'
ORIGIN = unreal.Vector(0, 0, 6000)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def main(name):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, ORIGIN, unreal.Rotator())
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(MESH))
    look = ORIGIN + unreal.Vector(0, 0, 300)
    cam_loc = look + unreal.Vector(2200, 2000, 1800)
    cap = EAS.spawn_actor_from_class(unreal.SceneCapture2D, cam_loc,
                                     unreal.MathLibrary.find_look_at_rotation(cam_loc, look))
    try:
        rt = unreal.RenderingLibrary.create_render_target2d(world, 1200, 800,
                                                            unreal.TextureRenderTargetFormat.RTF_RGBA8)
        comp = cap.capture_component2d
        comp.set_editor_property('texture_target', rt)
        comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property('fov_angle', 45)
        pp = comp.get_editor_property('post_process_settings')
        pp.set_editor_property('override_auto_exposure_method', True)
        pp.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
        pp.set_editor_property('override_auto_exposure_bias', True)
        pp.set_editor_property('auto_exposure_bias', 12.5)
        comp.set_editor_property('post_process_settings', pp)
        comp.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, rt, unreal.Paths.project_saved_dir() + 'TraegerC',
                                                     name + '.png')
    finally:
        cap.destroy_actor()
        actor.destroy_actor()
        unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 1')


main(globals().get('MAMMUT_SHOT', 'mammut_check'))
