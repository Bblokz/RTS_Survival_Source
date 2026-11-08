"""Review captures of the imported Träger C, assembled from TraegerC_sockets.json (run in the editor).

Spawns the parts above the open level, renders SceneCapture2D shots to Saved/TraegerC/<shot>.png, then
destroys everything it spawned, so the level is left as it was.
"""
import json

import unreal

# Copy of the Blender export's sockets file: the editor's Python may only read inside the project.
SOCKETS = 'D:/UE5Projects/RTS_Survival/Source/Tools/TraegerC/TraegerC_sockets.json'
MESHES = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/TraegerC/Meshes/'
OUT = unreal.Paths.project_saved_dir() + 'TraegerC'
ORIGIN = unreal.Vector(0, 0, 6000)  # over the map so its sun and sky light it
EXPOSURE_BIAS = 12.5
# shot: (camera offset from the look-at point, look-at point in airframe space, field of view, opened)
SHOTS = {
    'ue_overview': (unreal.Vector(5200, 6200, 5600), unreal.Vector(-2400, 0, 700), 40, False),
    'ue_bay_open': (unreal.Vector(2600, -2600, 1500), unreal.Vector(-1800, 0, 500), 40, True),
    'ue_nose': (unreal.Vector(1700, 1400, 900), unreal.Vector(-500, 0, 800), 45, False),
}
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def v(xyz):
    return unreal.Vector(*xyz)


def place(spawned, name, loc, pitch=0.0, roll=0.0, scale=None):
    actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, ORIGIN + loc, unreal.Rotator(roll=roll, pitch=pitch))
    actor.static_mesh_component.set_static_mesh(unreal.load_asset(MESHES + name))
    if scale is not None:
        actor.set_actor_scale3d(scale)
    spawned.append(actor)
    return actor


def assemble(spawned, opened):
    s = json.load(open(SOCKETS, encoding='utf-8'))
    place(spawned, 'SM_TraegerC_Airframe', unreal.Vector(0, 0, 0))
    for prop in s['props']:
        place(spawned, 'SM_TraegerC_Prop', v(prop['location']), roll=25.0 * prop['spin'])
    angle = s['bow_fairing_open_deg'] if opened else 0.0
    place(spawned, 'SM_TraegerC_BowFairing', v(s['bow_fairing_hinge']), pitch=angle)
    place(spawned, 'SM_TraegerC_TailFairing', v(s['tail_fairing_hinge']), pitch=-angle)
    drop = s['lowering_stroke_cm'] * 0.5 if opened else 0.0
    spreader = v(s['spreader_pivot']) - unreal.Vector(0, 0, drop)
    place(spawned, 'SM_TraegerC_Spreader', spreader)
    if drop:
        for anchor, eye in zip(s['cable_anchors'], s['cable_eyes_on_spreader']):
            a, e = v(anchor), spreader + v(eye)
            place(spawned, 'SM_TraegerC_Cable', a, scale=unreal.Vector(1, 1, (a.z - e.z) / 100.0))


def capture(world, name, offset, target, fov):
    rt = unreal.RenderingLibrary.create_render_target2d(world, 1600, 1000,
                                                        unreal.TextureRenderTargetFormat.RTF_RGBA8)
    look = ORIGIN + target
    cam_loc = look + offset
    cap = EAS.spawn_actor_from_class(unreal.SceneCapture2D, cam_loc,
                                     unreal.MathLibrary.find_look_at_rotation(cam_loc, look))
    comp = cap.capture_component2d
    comp.set_editor_property('texture_target', rt)
    comp.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property('fov_angle', fov)
    # A one-shot capture has no exposure history and comes out near black; fix the exposure instead.
    pp = comp.get_editor_property('post_process_settings')
    pp.set_editor_property('override_auto_exposure_method', True)
    pp.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
    pp.set_editor_property('override_auto_exposure_bias', True)
    pp.set_editor_property('auto_exposure_bias', EXPOSURE_BIAS)
    comp.set_editor_property('post_process_settings', pp)
    comp.set_editor_property('post_process_blend_weight', 1.0)
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, rt, OUT, name + '.png')
    cap.destroy_actor()
    unreal.log_warning('TRAEGER_CAPTURE ' + OUT + '/' + name + '.png')


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    try:
        for shot, (offset, target, fov, opened) in SHOTS.items():
            spawned = []
            try:
                assemble(spawned, opened)
                capture(world, shot, offset, target, fov)
            finally:
                for actor in spawned:
                    actor.destroy_actor()
    finally:
        unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 1')


main()
