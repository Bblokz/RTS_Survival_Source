"""Small scene-capture helper for headless (commandlet, -AllowCommandletRendering) look shots. Nothing is saved.

Pattern from GlobalAbilityIcons/ue_render_unit.py: show-only capture, manual exposure, no Lumen (ambient cubemap),
point/directional lights spawned far from anything, r.TextureStreaming 0 around the captures.
"""
import math
import os

import unreal

ACT = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
STUDIO = unreal.Vector(0.0, 0.0, 50000.0)
GROUND_RGB = (0.32, 0.28, 0.22)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def spawn(asset_path, loc=(0, 0, 0), yaw=0.0, scale=None):
    asset = unreal.load_asset(asset_path)
    where = STUDIO + unreal.Vector(*loc)
    rot = unreal.Rotator(0.0, 0.0, yaw)
    if isinstance(asset, unreal.Blueprint):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
        a = ACT.spawn_actor_from_class(cls, where, rot)
    else:
        a = ACT.spawn_actor_from_class(unreal.StaticMeshActor, where, rot)
        a.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        a.static_mesh_component.set_static_mesh(asset)
    if scale is not None:
        a.set_actor_scale3d(unreal.Vector(*scale))
    return a


def block(loc, size, rgb, yaw=0.0):
    """Scale block (cube) with a flat colour; loc = centre of the bottom face (cm), size in cm."""
    a = ACT.spawn_actor_from_class(unreal.StaticMeshActor, STUDIO + unreal.Vector(loc[0], loc[1], loc[2] + size[2] / 2),
                                   unreal.Rotator(0.0, 0.0, yaw))
    c = a.static_mesh_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    try:
        mi = c.create_dynamic_material_instance(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
        mi.set_vector_parameter_value('Color', unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    except Exception:
        pass
    return a


def ground(half=12000.0, rgb=GROUND_RGB):
    return block((0, 0, -10), (half * 2, half * 2, 10), rgb)


def lights(intensity=6.0):
    out = []
    sun = ACT.spawn_actor_from_class(unreal.DirectionalLight, STUDIO + unreal.Vector(0, 0, 3000),
                                     unreal.Rotator(0.0, -42.0, 200.0))     # light from the NNE, as on the map
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_intensity(intensity)
    sun.light_component.set_editor_property('cast_shadows', True)
    out.append(sun)
    fill = ACT.spawn_actor_from_class(unreal.DirectionalLight, STUDIO + unreal.Vector(0, 0, 3000),
                                      unreal.Rotator(0.0, -25.0, 20.0))
    fill.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    fill.light_component.set_intensity(intensity * 0.25)
    fill.light_component.set_editor_property('cast_shadows', False)
    out.append(fill)
    return out


def capture(path, eye, target, fov=60.0, res=(1280, 800), show=None, exposure=0.0, ambient=1.2):
    """eye / target relative to STUDIO (cm). show = actors (show-only list) or None (whole world)."""
    w = world()
    e = STUDIO + unreal.Vector(*eye)
    t = STUDIO + unreal.Vector(*target)
    rot = unreal.MathLibrary.find_look_at_rotation(e, t)
    cap_actor = ACT.spawn_actor_from_class(unreal.SceneCapture2D, e, rot)
    cap = cap_actor.capture_component2d
    rt = unreal.RenderingLibrary.create_render_target2d(w, res[0], res[1], unreal.TextureRenderTargetFormat.RTF_RGBA8,
                                                        unreal.LinearColor(0, 0, 0, 1))
    cap.set_editor_property('texture_target', rt)
    cap.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    if show is not None:
        cap.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        for a in show:
            cap.show_only_actor_components(a, True)
    cap.set_editor_property('fov_angle', fov)
    cap.set_editor_property('capture_every_frame', False)
    cap.set_editor_property('capture_on_movement', False)
    flags = [unreal.EngineShowFlagsSetting(show_flag_name=f, enabled=False)
             for f in ('Fog', 'AtmosphericFog', 'VolumetricFog', 'Bloom', 'LensFlares', 'Vignette', 'Grain',
                       'MotionBlur', 'DepthOfField')]
    cap.set_editor_property('show_flag_settings', flags)
    pp = cap.get_editor_property('post_process_settings')
    for k, v in (('override_auto_exposure_method', True), ('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL),
                 ('override_auto_exposure_bias', True), ('auto_exposure_bias', exposure),
                 ('override_auto_exposure_apply_physical_camera_exposure', True),
                 ('auto_exposure_apply_physical_camera_exposure', False),
                 ('override_dynamic_global_illumination_method', True),
                 ('dynamic_global_illumination_method', unreal.DynamicGlobalIlluminationMethod.NONE),
                 ('override_reflection_method', True), ('reflection_method', unreal.ReflectionMethod.SCREEN_SPACE),
                 ('ambient_cubemap', unreal.load_asset('/Engine/EngineResources/GrayLightTextureCube')),
                 ('override_ambient_cubemap_intensity', True), ('ambient_cubemap_intensity', ambient)):
        pp.set_editor_property(k, v)
    cap.set_editor_property('post_process_settings', pp)
    cap.set_editor_property('post_process_blend_weight', 1.0)
    cap.capture_scene()
    cap.capture_scene()
    d, f = os.path.split(path)
    os.makedirs(d, exist_ok=True)
    unreal.RenderingLibrary.export_render_target(w, rt, d, f)
    ACT.destroy_actor(cap_actor)
    return path


def rts_eye(target, dist=5000.0, pitch=55.0, yaw_from_north=-21.0):
    """Default RTS camera: looking NNW (forward yaw -21 from +X), pitch -55. Returns eye for a target (cm)."""
    fy = math.radians(yaw_from_north)
    p = math.radians(pitch)
    fwd = (math.cos(p) * math.cos(fy), math.cos(p) * math.sin(fy), -math.sin(p))
    return (target[0] - fwd[0] * dist, target[1] - fwd[1] * dist, target[2] - fwd[2] * dist)


def streaming(on):
    unreal.SystemLibrary.execute_console_command(None, 'r.TextureStreaming %d' % (1 if on else 0))
