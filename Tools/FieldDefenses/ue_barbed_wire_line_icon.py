# Step 1 of 3 of the HQ barbed wire line's option picture, in the style of the field trench's
# (T_BxpOption_GerFieldTrench): renders real SM_RTS_Barrier_BarbWire pieces laid along a bent line, seen from a steep
# angle on a flat ground, 512 px, to Saved/BarbedWireCaptures/T_BxpOption_GerBarbedWireLine_raw.png.
# Step 2: barbed_wire_line_icon_grade.py matches its ground to the trench picture's olive.
# Step 3: ue_barbed_wire_line_icon_import.py imports it and points SB_OptionImage_GerBarbedWireLine at it.
# Like Saved/StrikeDivision_BuildingLook/capture_nostream.py it builds its little scene far from the play area of the
# level the editor opened (that level's lighting and exposure apply) and removes it again; texture streaming is off
# so the freshly spawned meshes are not texture starved.
# Needs a renderer: run in a full editor with -RenderOffscreen (not -nullrhi) and -ExecutePythonScript=<this file>;
# the script quits the editor when it is done. Check GPU memory first (memory: gpu-oom-crashes-other-editors).
import math
import os

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Shorts/Saved/BarbedWireCaptures'
RAW_PICTURE_NAME = 'T_BxpOption_GerBarbedWireLine_raw'
WIRE_MESH_PATH = '/Game/RTS_Survival/Meshes/Military/Barriers/SM_RTS_Barrier_BarbWire'
PICTURE_SIZE = 512
PIECE_LENGTH = 400.0
# Far from any play area, above anything the level holds there.
SPOT = unreal.Vector(0, 400000, 20000)
# Close to the trench picture's olive; the grade step makes it exact.
GROUND_COLOUR = unreal.LinearColor(0.28, 0.29, 0.17, 1)
# A bend and a kink like the trench picture: headings in degrees, one per piece.
PIECE_YAWS = [-15.0, -15.0, 0.0, 20.0, 40.0, 40.0, 15.0, 0.0]
CAMERA_PITCH_DEGREES = 62.0
CAMERA_YAW_DEGREES = 200.0
FOV_DEGREES = 40.0
# How much wider the view is than the line: the line fills about 80 % of the picture, as the trench fills its own.
FRAMING_DISTANCE = 1.3
LOG_PREFIX = 'BARBED_WIRE_ICON '


def log(message):
    unreal.log(LOG_PREFIX + message)


def spawn_mesh(actors, mesh, location, yaw):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(roll=0, pitch=0, yaw=yaw))
    actor.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.static_mesh_component.set_static_mesh(mesh)
    return actor


def lay_wire(actors, mesh):
    pieces = []
    point = SPOT
    for yaw in PIECE_YAWS:
        direction = unreal.Vector(math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0)
        pieces.append(spawn_mesh(actors, mesh, point + direction * (PIECE_LENGTH * 0.5), yaw))
        point = point + direction * PIECE_LENGTH
    return pieces


def set_up_scene(actors):
    spawned = []
    plane = actors.spawn_actor_from_class(unreal.StaticMeshActor, SPOT)
    plane.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    plane.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    plane.set_actor_scale3d(unreal.Vector(600, 600, 1))
    material = plane.static_mesh_component.create_dynamic_material_instance(
        0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
    material.set_vector_parameter_value('Color', GROUND_COLOUR)
    spawned.append(plane)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, SPOT, unreal.Rotator(roll=0, pitch=-55, yaw=140))
    sun.light_component.set_intensity(6)
    spawned.append(sun)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, SPOT)
    sky.light_component.set_intensity(1.5)
    sky.light_component.recapture_sky()
    spawned.append(sky)
    return spawned


def get_line_bounds(pieces):
    bounds_min = None
    bounds_max = None
    for piece in pieces:
        origin, extent = piece.get_actor_bounds(False)
        low = origin - extent
        high = origin + extent
        bounds_min = low if bounds_min is None else unreal.Vector(min(bounds_min.x, low.x), min(bounds_min.y, low.y),
                                                                min(bounds_min.z, low.z))
        bounds_max = high if bounds_max is None else unreal.Vector(max(bounds_max.x, high.x),
                                                                 max(bounds_max.y, high.y), max(bounds_max.z, high.z))
    return (bounds_min + bounds_max) * 0.5, (bounds_max - bounds_min) * 0.5


def capture(actors, world, pieces):
    centre, half_extent = get_line_bounds(pieces)
    render_target = unreal.RenderingLibrary.create_render_target2d(
        world, PICTURE_SIZE, PICTURE_SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    camera = actors.spawn_actor_from_class(unreal.SceneCapture2D, centre)
    component = camera.capture_component2d
    component.texture_target = render_target
    component.fov_angle = FOV_DEGREES
    component.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    distance = max(half_extent.x, half_extent.y) * FRAMING_DISTANCE / math.tan(math.radians(FOV_DEGREES * 0.5))
    pitch = math.radians(CAMERA_PITCH_DEGREES)
    yaw = math.radians(CAMERA_YAW_DEGREES)
    eye = centre + unreal.Vector(math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw),
                                 math.sin(pitch)) * distance
    camera.set_actor_location(eye, False, False)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, centre), False)
    unreal.SystemLibrary.execute_console_command(world, 'FlushRenderingCommands')
    log('line centre {} half extent {} camera distance {:.0f}'.format(centre, half_extent, distance))
    component.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, render_target, OUT_DIR, RAW_PICTURE_NAME + '.png')
    actors.destroy_actor(camera)
    return OUT_DIR + '/' + RAW_PICTURE_NAME + '.png'


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    mesh = unreal.load_asset(WIRE_MESH_PATH)
    spawned = []
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    try:
        spawned = set_up_scene(actors)
        pieces = lay_wire(actors, mesh)
        spawned += pieces
        log('captured ' + capture(actors, world, pieces))
    finally:
        unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 1')
        for actor in spawned:
            actors.destroy_actor(actor)


try:
    main()
except Exception as error:  # noqa: BLE001 - logged for the headless run
    unreal.log_error(LOG_PREFIX + 'failed: {}'.format(error))
finally:
    unreal.SystemLibrary.quit_editor()
