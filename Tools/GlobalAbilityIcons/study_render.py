"""Renders for the icon look study (study_pbr_looks.py). Run in the open editor:
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/study_render.py").read())
    study_all()            # every variant twice (the first capture can catch unstreamed textures)
Writes Saved/GlobalAbilityIcons/study/renders_<variant>/<job>_<pass>.png with the same names the composer reads.
The shared render jobs and library are only read, never changed. Run it in a separate editor on
/Engine/Maps/Templates/Template_Default: the relight and scene variants turn that map's sun.
"""
import math

exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/render_ability_parts.py").read())

STUDY = r"D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/study"
STUDY_JOBS = ["Ju87_Dive", "PE8_Level", "T3476", "T3485"]
BUFFER_VIS = "/Engine/BufferVisualization/"
# One warm low sun behind the subject on the camera's left: rim light and a terminator across the camo, matching the
# warm glow low in the icon skies. Cool sky fill from above, a weak warm bounce from the ground below.
# The key is the map's sun (set_sun) with the real sky light of its atmosphere; these add a warm rim from behind
# and a weak warm bounce from the ground.
RELIGHT_LIGHTS = (
    (160.0, 22.0, 1.1, unreal.LinearColor(1.0, 0.8, 0.58, 1.0)),
    (20.0, -35.0, 0.18, unreal.LinearColor(1.0, 0.82, 0.62, 1.0)),
)
SUN_AZIMUTH_FROM_CAMERA = -40.0     # in front of the subject, on the camera's left
SUN_ELEVATION = 32.0
SUN_LUX = 14.0
RELIGHT_CUBEMAP = "/Engine/MapTemplates/Sky/Desert_Outer_HDR"
BASE_LIGHTS = STUDIO_LIGHTS


def _capture_passes(out_name, actors, comps, spawned, passes, res, azimuth, elevation, fov, padding,
                    light_intensity, ambient_intensity, ambient_cubemap):
    """_capture_staged with a free list of passes: (suffix, capture source, post-process material or None)."""
    world = _world()
    center, radius = _bounds(comps)
    spawned.extend(_spawn_studio_lights(center, radius, azimuth, light_intensity))
    dist = radius * padding / math.sin(math.radians(fov) * 0.5)
    az, el = math.radians(azimuth), math.radians(elevation)
    cam_loc = center + unreal.Vector(math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)) * dist
    cam_rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, center)
    _write_points(out_name, actors, spawned[0], POINTS, cam_loc, cam_rot, fov, res)
    for suffix, source, material in passes:
        cap_actor, cap, rt = _make_capture(world, res, source, unreal.TextureRenderTargetFormat.RTF_RGBA8, actors,
                                           cam_loc, cam_rot, fov, 0.0, ambient_cubemap, ambient_intensity)
        spawned.append(cap_actor)
        if material:
            pp = cap.get_editor_property("post_process_settings")
            pp.set_editor_property("weighted_blendables", unreal.WeightedBlendables(
                [unreal.WeightedBlendable(1.0, unreal.load_asset(BUFFER_VIS + material))]))
            cap.set_editor_property("post_process_settings", pp)
        cap.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, rt, RENDER_DIR, "%s_%s.png" % (out_name, suffix))
    # camera axes, so the composer can turn world normals into screen space
    f, r, u = cam_rot.get_forward_vector(), cam_rot.get_right_vector(), cam_rot.get_up_vector()
    with open("%s/%s_camera.json" % (RENDER_DIR, out_name), "w") as fh:
        fh.write(repr({"forward": [f.x, f.y, f.z], "right": [r.x, r.y, r.z], "up": [u.x, u.y, u.z]}).replace("'", '"'))
    print("STUDY_RENDERED", RENDER_DIR.split("/")[-1], out_name)


COLOR_PASSES = [("color", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR, None),
                ("mask", unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR, None)]
# Buffer-visualization blendables (roughness, metallic) did not apply to the capture, so normals + base colour only.
GBUFFER_PASSES = COLOR_PASSES + [("normal", unreal.SceneCaptureSource.SCS_NORMAL, None),
                                 ("basecolor", unreal.SceneCaptureSource.SCS_BASE_COLOR, None)]

# variant: (passes, light rig, cubemap, ambient intensity, job overrides)
VARIANTS = {
    "gbuffer": (GBUFFER_PASSES, BASE_LIGHTS, None, None, {}),
    "relight": (COLOR_PASSES, RELIGHT_LIGHTS, RELIGHT_CUBEMAP, 0.6, {}),
    # close camera, wide lens: the same framing from a third of the distance gives real perspective
    "lens": (COLOR_PASSES, BASE_LIGHTS, None, None, {"fov": 40.0}),
}


def set_sun(camera_azimuth, on=True):
    """Points the map's sun from SUN_AZIMUTH_FROM_CAMERA / SUN_ELEVATION as seen from the camera, or dims it."""
    for actor in _actor_sub().get_all_level_actors():
        if isinstance(actor, unreal.DirectionalLight):
            yaw = camera_azimuth + SUN_AZIMUTH_FROM_CAMERA + 180.0
            actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-SUN_ELEVATION, yaw=yaw), False)
            actor.light_component.set_intensity(SUN_LUX if on else 0.0)
        if isinstance(actor, unreal.SkyLight):
            actor.light_component.set_intensity(1.0 if on else 0.0)
            actor.light_component.recapture_sky()


def study_variant(variant, names=None):
    global RENDER_DIR, STUDIO_LIGHTS
    passes, lights, cubemap, ambient, overrides = VARIANTS[variant]
    RENDER_DIR = STUDY + "/renders_" + variant
    STUDIO_LIGHTS = lights
    try:
        for name in names or STUDY_JOBS:
            job = dict(LOOK, **JOBS[name])
            job.update(overrides)
            # the base and lens looks match the shipped icons: no map sun, only the studio rig
            set_sun(job["azimuth"], on=variant == "relight")
            parts = job.pop("parts")
            spawned = []
            try:
                actors, comps = _stage_parts(parts, spawned)
                _capture_passes(name, actors, comps, spawned, passes, job.get("res", 2048), job["azimuth"],
                                job["elevation"], job.get("fov", CAMERA_FOV_DEG), job.get("padding", 1.04),
                                job.get("light_intensity", 8.0),
                                ambient if ambient is not None else job.get("ambient_intensity", 2.5),
                                cubemap or AMBIENT_CUBEMAP)
            finally:
                _destroy(spawned)
    finally:
        STUDIO_LIGHTS = BASE_LIGHTS


# --- whole scenes: formation, sky atmosphere, clouds and the sun, captured with everything visible -----------------
SCENES = {
    # Ju 87s seen from slightly below so the frame is sky: lead plane at the origin, wingmen behind on either side
    # offsets are camera right / depth solved for the shipped layout: one wingman lower left, one upper right
    "Scene_Ju87": dict(parts=[dict(asset=JU87, loc=(-1290.0, 630.0, -150.0), yaw=0.0, pitch=-22.0, roll=-12.0, anim=JU87_FLY),
                              dict(asset=JU87, loc=(-260.0, -1460.0, 650.0), yaw=0.0, pitch=-22.0, roll=-12.0, anim=JU87_FLY),
                              dict(asset=JU87, yaw=0.0, pitch=-22.0, roll=-12.0, anim=JU87_FLY)],
                       azimuth=35.0, elevation=-14.0, padding=0.62, guns=("W1", "W2")),
    "Scene_T34Drop": dict(parts=[dict(asset=PE8, loc=(-2600.0, -100.0, 1500.0), yaw=0.0, pitch=2.0, roll=-5.0, anim=PE8_FLY),
                                 dict(asset=T34 + "Rus_Vehicle_T3476", loc=(-2600.0, -100.0, 1105.0), yaw=0.0, pitch=2.0, roll=-5.0),
                                 dict(asset=PE8, loc=(-1000.0, -2500.0, 1400.0), yaw=0.0, pitch=2.0, roll=-5.0, anim=PE8_FLY),
                                 dict(asset=T34 + "Rus_Vehicle_T3476", loc=(-1000.0, -2500.0, 1005.0), yaw=0.0, pitch=2.0, roll=-5.0),
                                 dict(asset=PE8, yaw=0.0, pitch=2.0, roll=-5.0, anim=PE8_FLY),
                                 dict(asset=T34 + "Rus_Vehicle_T3485", loc=(0.0, 0.0, -395.0), yaw=0.0, pitch=2.0, roll=-5.0)],
                          azimuth=60.0, elevation=-12.0, padding=0.62, guns=()),
}


def _project_parts(out_name, parts_actors, guns, cam_loc, cam_rot, fov, res):
    """Screen points for every staged part: its origin, forward and its gun sockets."""
    rows = []
    for actors in parts_actors:
        root = actors[0]
        row = {"origin": _project(root.get_actor_location(), cam_loc, cam_rot, fov, res),
               "forward": _project(root.get_actor_location() + root.get_actor_forward_vector() * 300.0, cam_loc, cam_rot,
                                   fov, res)}
        for a in actors:
            for c in a.get_components_by_class(unreal.MeshComponent):
                for g in guns:
                    if c.does_socket_exist(g):
                        row[g] = _project(c.get_socket_location(g), cam_loc, cam_rot, fov, res)
        rows.append(row)
    with open("%s/%s_points.json" % (RENDER_DIR, out_name), "w") as fh:
        fh.write(repr(rows).replace("'", '"'))


def study_scene(name, res=1024, exposure_bias=1.2):
    global RENDER_DIR
    RENDER_DIR = STUDY + "/renders_scene"
    job = dict(SCENES[name])
    set_sun(job["azimuth"], on=True)
    spawned, parts_actors, comps = [], [], []
    try:
        for part in job["parts"]:
            first = len(spawned)
            actors, part_comps = _stage_parts([part], spawned)
            parts_actors.append(actors)
            comps.extend(part_comps)
        world = _world()
        center, radius = _bounds(comps)
        fov = CAMERA_FOV_DEG
        dist = radius * job["padding"] / math.sin(math.radians(fov) * 0.5)
        az, el = math.radians(job["azimuth"]), math.radians(job["elevation"])
        cam_loc = center + unreal.Vector(math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)) * dist
        cam_rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, center)
        _project_parts(name, parts_actors, job["guns"], cam_loc, cam_rot, fov, res)
        cap_actor, cap, rt = _make_capture(world, res, unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
                                           unreal.TextureRenderTargetFormat.RTF_RGBA8, [], cam_loc, cam_rot, fov,
                                           exposure_bias, AMBIENT_CUBEMAP, 0.0,
                                           gi_method=unreal.DynamicGlobalIlluminationMethod.LUMEN,
                                           reflection_method=unreal.ReflectionMethod.LUMEN)
        spawned.append(cap_actor)
        # everything in the world renders: sky atmosphere, volumetric clouds, height fog
        cap.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_RENDER_SCENE_PRIMITIVES)
        cap.set_editor_property("show_flag_settings", [])
        cap.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, rt, RENDER_DIR, name + "_color.png")
        print("STUDY_RENDERED scene", name, "radius %.0f" % radius)
    finally:
        _destroy(spawned)


def study_all():
    for _ in range(2):
        for variant in VARIANTS:
            study_variant(variant)
        for scene in SCENES:
            study_scene(scene)
