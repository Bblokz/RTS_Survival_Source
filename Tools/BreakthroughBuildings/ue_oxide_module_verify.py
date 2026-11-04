"""Proves the Gesenkhammer module's forge materials move: spawns the four meshes far from everything in the open editor
world, freezes the materials at chosen times (DebugTime on dynamic instances), captures each pose with a SceneCapture2D
and writes PNGs to Saved/OxideModuleVerify, then deletes everything it spawned. Nothing is saved.
Run with the UECP execute_python action. TIMES and VIEWS can be set before exec.
"""
import unreal

HOME = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughOxideWorks/Gesenkhammer'
NAME = 'SM_BF_OxideWorks_Gesenkhammer'
OUT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + 'OxideModuleVerify'
ORIGIN = unreal.Vector(400000.0, 400000.0, 30000.0)   # far from any map content
TIMES = globals().get('TIMES', (0.0, 0.75, 1.5, 2.2, 7.55))
VIEWS = globals().get('VIEWS', {
    # name: (camera direction from the module towards the camera, look-at point in the module frame, ortho width cm)
    'bin_end': (unreal.Vector(0.50, 0.69, 0.52), unreal.Vector(180, 330, 170), 800.0),      # the RTS camera's side in BXP1
    'hammer': (unreal.Vector(1.0, 0.25, 0.35), unreal.Vector(280, 0, 250), 700.0),
})
# base colour shows the geometry unlit, which is what a pose check needs; FINAL shows the glow under the map's lighting
SOURCE = globals().get('SOURCE', 'SCS_BASE_COLOR')


def spawn():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors, mids = [], []
    for part in ('', '_Moving', '_Hot', '_Parts'):
        actor = subsystem.spawn_actor_from_class(unreal.StaticMeshActor, ORIGIN, unreal.Rotator(0, 0, 0))
        actor.set_actor_label('ZZ_OxideModuleVerify' + part)
        comp = actor.static_mesh_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_static_mesh(unreal.load_asset(f'{HOME}/{NAME}{part}'))
        if part in ('_Moving', '_Hot'):
            mid = comp.create_dynamic_material_instance(0)
            mids.append(mid)
            if globals().get('STOPPED'):    # custom primitive data 16 is MotionStopped: unset (0) runs, 1 holds the rest pose
                comp.set_custom_primitive_data_float(16, 1.0)
        actors.append(actor)
    return actors, mids


def look(capture, direction, target, width):
    d = direction.normal()
    where = ORIGIN + target + d * 5000.0
    rot = unreal.MathLibrary.find_look_at_rotation(where, ORIGIN + target)
    capture.set_actor_location_and_rotation(where, rot, False, False)
    comp = capture.capture_component2d
    comp.set_editor_property('ortho_width', width)


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors, mids = spawn()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    capture = subsystem.spawn_actor_from_class(unreal.SceneCapture2D, ORIGIN, unreal.Rotator(0, 0, 0))
    comp = capture.capture_component2d
    rt = unreal.RenderingLibrary.create_render_target2d(world, 1200, 900, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    comp.set_editor_property('texture_target', rt)
    comp.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    comp.set_editor_property('capture_source', getattr(unreal.SceneCaptureSource, SOURCE))
    comp.set_editor_property('capture_every_frame', False)
    comp.set_editor_property('capture_on_movement', False)
    written = []
    try:
        for view, (direction, target, width) in VIEWS.items():
            look(capture, direction, target, width)
            for t in TIMES:
                for mid in mids:
                    mid.set_scalar_parameter_value('DebugTime', t)
                comp.capture_scene()
                name = f'{SOURCE[4:].lower()}_{view}_t{t:05.2f}.png'.replace('.', '_', 1)
                unreal.RenderingLibrary.export_render_target(world, rt, OUT, name)
                written.append(name)
    finally:
        for a in actors + [capture]:
            subsystem.destroy_actor(a)
    gate = [(str(p.get_editor_property('parameter_name')), p.get_editor_property('default_value'))
            for p in unreal.load_asset('/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/MasterMats/MPC_RTSWPO').get_editor_property('scalar_parameters')]
    print('VERIFY wrote', len(written), 'to', OUT, written)
    print('VERIFY MPC_RTSWPO', gate)
    left = [a.get_actor_label() for a in subsystem.get_all_level_actors() if a.get_actor_label().startswith('ZZ_OxideModuleVerify')]
    print('VERIFY leftovers', left)


main()
