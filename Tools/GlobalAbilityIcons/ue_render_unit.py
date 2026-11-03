"""Render units/meshes from the live UE editor to cut-out PNGs for tech icons.

Run inside the editor (UECP execute_python):
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read())
    render_unit("/Game/.../Ger_Harvester_Panzer1", "Harvester")
    render_scene("Scav", [dict(asset=WRECK), dict(asset=SOLDIER, loc=(150, 80, 0), yaw=200, anim=DRILL, anim_time=0.6)])

Parts are spawned far above the open map, captured with a show-only SceneCapture2D
(one tonemapped colour pass + one HDR pass whose alpha is the coverage mask), then deleted.
Nothing is saved. compose_tech_icons.py merges <name>_color.png + <name>_mask.png.
Render every batch twice: the first capture after an asset loads can catch unstreamed (flat grey) textures.
"""
import math

import unreal

RENDER_DIR = r"D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/renders"
STUDIO_LOCATION = unreal.Vector(0.0, 0.0, 120000.0)
# Camera sits on the sun side of the level light (Sun yaw 150 -> light comes from yaw -30).
CAMERA_AZIMUTH_DEG = -30.0
CAMERA_ELEVATION_DEG = 28.0
CAMERA_FOV_DEG = 18.0
# Unit master materials are metallic: a neutral grey environment keeps their real albedo.
AMBIENT_CUBEMAP = "/Engine/EngineResources/GrayLightTextureCube"


def _actor_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _spawn_asset(asset_path, yaw, loc=(0.0, 0.0, 0.0), pitch=0.0, roll=0.0, anim_path=None, anim_time=0.0):
    rot = unreal.Rotator(roll, pitch, yaw)  # unreal.Rotator(...) positional order is (roll, pitch, yaw)
    where = STUDIO_LOCATION + unreal.Vector(*loc)
    asset = unreal.load_asset(asset_path)
    if isinstance(asset, unreal.Blueprint):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
        return _actor_sub().spawn_actor_from_class(cls, where, rot)
    if isinstance(asset, unreal.StaticMesh):
        actor = _actor_sub().spawn_actor_from_class(unreal.StaticMeshActor, where, rot)
        actor.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        actor.static_mesh_component.set_static_mesh(asset)
        return actor
    if isinstance(asset, unreal.SkeletalMesh):
        actor = _actor_sub().spawn_actor_from_class(unreal.SkeletalMeshActor, where, rot)
        comp = actor.skeletal_mesh_component
        if anim_path:
            # Set the single-node animation data BEFORE the mesh: assigning the mesh runs InitAnim, which evaluates
            # the pose at the saved position right away (editor actors otherwise stay in reference pose until a tick).
            comp.set_update_animation_in_editor(True)
            comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            comp.override_animation_data(unreal.load_asset(anim_path), False, False, anim_time, 0.0)
        comp.set_skinned_asset_and_update(asset, True)
        return actor
    raise RuntimeError(f"Unsupported asset type for {asset_path}: {type(asset)}")


def _pose_component(actor):
    """SkeletalMeshActor -> its component; Characters (squad unit BPs) -> their first skeletal mesh."""
    if isinstance(actor, unreal.SkeletalMeshActor):
        return actor.skeletal_mesh_component
    return actor.get_component_by_class(unreal.SkeletalMeshComponent)


def _pose(actor, anim_path, anim_time):
    comp = _pose_component(actor)
    comp.set_update_animation_in_editor(True)
    comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    comp.set_animation(unreal.load_asset(anim_path))
    comp.set_position(anim_time, False)
    comp.set_play_rate(0.0)


def _collect_actors(root):
    actors = [root]
    for comp in root.get_components_by_class(unreal.ChildActorComponent):
        child = comp.get_editor_property("child_actor")
        if child:
            actors.extend(_collect_actors(child))
    for att in root.get_attached_actors():
        if att not in actors:
            actors.extend(_collect_actors(att))
    return actors


def _find_component(actors, name):
    for a in actors:
        for c in a.get_components_by_class(unreal.SceneComponent):
            if c.get_name() == name:
                return c
    raise RuntimeError(f"No component {name} on {[a.get_name() for a in actors]}")


def _visible_mesh_components(actors, hide_patterns):
    comps = []
    for a in actors:
        for c in a.get_components_by_class(unreal.MeshComponent):
            name = c.get_name()
            if isinstance(c, unreal.WidgetComponent) or any(p.lower() in name.lower() for p in hide_patterns):
                c.set_visibility(False, False)
                continue
            if not c.is_visible() or c.get_editor_property("hidden_in_game"):
                continue
            c.prestream_textures(60.0, True)
            comps.append(c)
    return comps


def _bounds(comps):
    lo = [1e18] * 3
    hi = [-1e18] * 3
    for c in comps:
        origin, extent, _ = unreal.SystemLibrary.get_component_bounds(c)
        for i, (o, e) in enumerate(((origin.x, extent.x), (origin.y, extent.y), (origin.z, extent.z))):
            lo[i] = min(lo[i], o - e)
            hi[i] = max(hi[i], o + e)
    center = unreal.Vector(*[(l + h) * 0.5 for l, h in zip(lo, hi)])
    radius = math.sqrt(sum(((h - l) * 0.5) ** 2 for l, h in zip(lo, hi)))
    return center, radius


def _make_capture(world, res, source, fmt, show_actors, cam_loc, cam_rot, fov, exposure_bias,
                  ambient_cubemap, ambient_intensity,
                  gi_method=unreal.DynamicGlobalIlluminationMethod.NONE,
                  reflection_method=unreal.ReflectionMethod.SCREEN_SPACE):
    cap_actor = _actor_sub().spawn_actor_from_class(unreal.SceneCapture2D, cam_loc, cam_rot)
    cap = cap_actor.capture_component2d
    rt = unreal.RenderingLibrary.create_render_target2d(world, res, res, fmt, unreal.LinearColor(0, 0, 0, 0))
    cap.set_editor_property("texture_target", rt)
    cap.set_editor_property("capture_source", source)
    cap.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    for a in show_actors:
        cap.show_only_actor_components(a, False)
    cap.set_editor_property("fov_angle", fov)
    cap.set_editor_property("capture_every_frame", False)
    cap.set_editor_property("capture_on_movement", False)
    flags = []
    for flag in ("Fog", "AtmosphericFog", "VolumetricFog", "Atmosphere", "Bloom", "LensFlares", "Vignette", "Grain",
                 "MotionBlur", "DepthOfField"):
        flags.append(unreal.EngineShowFlagsSetting(show_flag_name=flag, enabled=False))
    cap.set_editor_property("show_flag_settings", flags)
    pp = cap.get_editor_property("post_process_settings")
    pp.set_editor_property("override_auto_exposure_method", True)
    pp.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    pp.set_editor_property("override_auto_exposure_bias", True)
    pp.set_editor_property("auto_exposure_bias", exposure_bias)
    pp.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
    pp.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
    # Lumen has nothing to trace in a show-only capture: metals go black. Use the ambient cubemap instead.
    pp.set_editor_property("override_dynamic_global_illumination_method", True)
    pp.set_editor_property("dynamic_global_illumination_method", gi_method)
    pp.set_editor_property("override_reflection_method", True)
    pp.set_editor_property("reflection_method", reflection_method)
    pp.set_editor_property("ambient_cubemap", unreal.load_asset(ambient_cubemap))
    pp.set_editor_property("override_ambient_cubemap_intensity", True)
    pp.set_editor_property("ambient_cubemap_intensity", ambient_intensity)
    pp.set_editor_property("override_ambient_cubemap_tint", True)
    pp.set_editor_property("ambient_cubemap_tint", unreal.LinearColor(1, 1, 1, 1))
    cap.set_editor_property("post_process_settings", pp)
    cap.set_editor_property("post_process_blend_weight", 1.0)
    return cap_actor, cap, rt


# (azimuth offset from camera, elevation, intensity scale, colour)
STUDIO_LIGHTS = (
    (-35.0, 45.0, 1.0, unreal.LinearColor(1.0, 0.95, 0.88, 1.0)),   # key, camera-left and high
    (60.0, 15.0, 0.35, unreal.LinearColor(0.80, 0.88, 1.0, 1.0)),   # fill, camera-right and low
    (170.0, 35.0, 0.9, unreal.LinearColor(1.0, 1.0, 1.0, 1.0)),     # rim, behind the unit
)


def _spawn_studio_lights(center, radius, cam_azimuth, base_intensity):
    lights = []
    for az_off, el, scale, colour in STUDIO_LIGHTS:
        az, elr = math.radians(cam_azimuth + az_off), math.radians(el)
        d = radius * 3.0
        loc = center + unreal.Vector(math.cos(elr) * math.cos(az), math.cos(elr) * math.sin(az), math.sin(elr)) * d
        light = _actor_sub().spawn_actor_from_class(unreal.PointLight, loc, unreal.Rotator(0, 0, 0))
        comp = light.point_light_component
        comp.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        comp.set_intensity(base_intensity * scale * (d / 100.0) ** 2)  # base_intensity = lux at the unit
        comp.set_light_color(colour)
        comp.set_attenuation_radius(d * 2.5)
        comp.set_editor_property("cast_shadows", True)
        lights.append(light)
    return lights


def _project(world_pos, cam_loc, cam_rot, fov, res):
    """World position -> pixel (x, y) in the square capture."""
    d = world_pos - cam_loc
    f, r, u = cam_rot.get_forward_vector(), cam_rot.get_right_vector(), cam_rot.get_up_vector()
    depth = d.dot(f)
    t = math.tan(math.radians(fov) * 0.5)
    x = d.dot(r) / depth / t
    y = d.dot(u) / depth / t
    return [round((x + 1) * 0.5 * res, 1), round((1 - y) * 0.5 * res, 1)]


def _write_points(out_name, actors, root, sockets, cam_loc, cam_rot, fov, res):
    pts = {}
    for a in actors:
        for c in a.get_components_by_class(unreal.MeshComponent):
            for sname in c.get_all_socket_names():
                if str(sname) in sockets:
                    pts[str(sname)] = _project(c.get_socket_location(sname), cam_loc, cam_rot, fov, res)
    origin = root.get_actor_location()
    pts["origin"] = _project(origin, cam_loc, cam_rot, fov, res)
    pts["forward"] = _project(origin + root.get_actor_forward_vector() * 300.0, cam_loc, cam_rot, fov, res)
    pts["right"] = _project(origin + root.get_actor_right_vector() * 300.0, cam_loc, cam_rot, fov, res)
    pts["up"] = _project(origin + root.get_actor_up_vector() * 300.0, cam_loc, cam_rot, fov, res)
    with open(f"{RENDER_DIR}/{out_name}_points.json", "w") as fh:
        fh.write(repr(pts).replace("'", '"'))
    print("POINTS", out_name, pts)


def clear_studio():
    """Destroy every actor left in the studio volume (e.g. after keep=True or a failed run)."""
    for a in _actor_sub().get_all_level_actors():
        if a.get_actor_location().z > STUDIO_LOCATION.z * 0.5:
            _actor_sub().destroy_actor(a)


def _stage_parts(parts, spawned):
    """Spawn and pose every part; spawned actors are appended to `spawned` (cleanup list)."""
    actors, comps = [], []
    for part in parts:
        pose_at_spawn = bool(part.get("anim")) and part.get("pose_at_spawn", True)
        actor = _spawn_asset(part["asset"], part.get("yaw", -60.0), part.get("loc", (0.0, 0.0, 0.0)),
                             part.get("pitch", 0.0), part.get("roll", 0.0),
                             part.get("anim") if pose_at_spawn else None, part.get("anim_time", 0.0))
        spawned.append(actor)
        if "scale" in part:
            sc = part["scale"]
            actor.set_actor_scale3d(unreal.Vector(*sc) if isinstance(sc, tuple) else unreal.Vector(sc, sc, sc))
        for bone in part.get("hide_bones", ()):
            actor.get_component_by_class(unreal.SkeletalMeshComponent).hide_bone_by_name(
                bone, unreal.PhysBodyOp.PBO_NONE)
        for comp_pattern, class_path in part.get("child_actors", {}).items():
            for c in actor.get_components_by_class(unreal.ChildActorComponent):
                if comp_pattern.lower() in c.get_name().lower():
                    c.set_child_actor_class(unreal.EditorAssetLibrary.load_blueprint_class(class_path))
        for slot, material_path in part.get("materials", ()):
            for c in actor.get_components_by_class(unreal.MeshComponent):
                c.set_material(slot, unreal.load_asset(material_path))
        if part.get("anim") and not (pose_at_spawn and isinstance(actor, unreal.SkeletalMeshActor)):
            _pose(actor, part["anim"], part.get("anim_time", 0.0))
        if part.get("attach_to"):
            parent = _find_component(_collect_actors(spawned[0]), part["attach_to"])
            actor.root_component.attach_to_component(
                parent, part.get("attach_socket", ""), unreal.AttachmentRule.KEEP_RELATIVE,
                unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE, False)
            rel = unreal.Transform(unreal.Vector(*part.get("rel_loc", (0.0, 0.0, 0.0))),
                                   unreal.Rotator(*part.get("rel_rot", (0.0, 0.0, 0.0))))
            actor.root_component.set_relative_transform(rel, False, False)
        part_actors = [a for a in _collect_actors(actor) if a not in actors]
        for a in part_actors:
            for c in a.get_components_by_class(unreal.MeshComponent):
                if any(p.lower() in c.get_name().lower() for p in part.get("show", ())):
                    c.set_visibility(True, False)
        actors.extend(part_actors)
        comps.extend(_visible_mesh_components(part_actors, part.get("hide", ())))
    return actors, comps


def _capture_staged(out_name, actors, comps, spawned, res=1024, azimuth=CAMERA_AZIMUTH_DEG,
                    elevation=CAMERA_ELEVATION_DEG, fov=CAMERA_FOV_DEG, padding=1.04, exposure_bias=0.0,
                    light_intensity=8.0, ambient_intensity=2.5, ambient_cubemap=None, debug=False, sockets=None, normals=False, isolate=None):
    world = _world()
    center, radius = _bounds(comps)
    if debug:
        for c in comps:
            o, e, r = unreal.SystemLibrary.get_component_bounds(c)
            print("COMP", c.get_owner().get_name(), c.get_name(), c.get_class().get_name(), "r=%.0f" % r)
    spawned.extend(_spawn_studio_lights(center, radius, azimuth, light_intensity))
    if isolate:  # camera framed on the whole unit, but only these components render (region masks)
        for c in comps:
            if not any(k.lower() in c.get_name().lower() for k in isolate):
                c.set_visibility(False, False)
    dist = radius * padding / math.sin(math.radians(fov) * 0.5)
    az, el = math.radians(azimuth), math.radians(elevation)
    offset = unreal.Vector(math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)) * dist
    cam_loc = center + offset
    cam_rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, center)
    if sockets is not None:  # screen-space anchors for 2D effects (muzzle flash, motion streaks)
        _write_points(out_name, actors, spawned[0], sockets, cam_loc, cam_rot, fov, res)

    outputs = []
    passes = [("color", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR),
              ("mask", unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)]
    if normals:  # world normals: plate edges without the camo texture (compose armor_glow)
        passes.append(("normal", unreal.SceneCaptureSource.SCS_NORMAL))
    for suffix, source in passes:
        cap_actor, cap, rt = _make_capture(world, res, source, unreal.TextureRenderTargetFormat.RTF_RGBA8, actors,
                                           cam_loc, cam_rot, fov, exposure_bias,
                                           ambient_cubemap or AMBIENT_CUBEMAP, ambient_intensity)
        spawned.append(cap_actor)
        cap.capture_scene()
        unreal.RenderingLibrary.export_render_target(world, rt, RENDER_DIR, f"{out_name}_{suffix}.png")
        outputs.append(f"{RENDER_DIR}/{out_name}_{suffix}.png")
    print("RENDERED", out_name, "radius %.0f" % radius, "components", len(comps))
    return outputs


def _destroy(spawned):
    for a in reversed(spawned):
        if a:
            _actor_sub().destroy_actor(a)


def render_scene(out_name, parts, keep=False, **kwargs):
    """Stage several parts and capture them together.

    part keys: asset (BP / StaticMesh / SkeletalMesh path), loc (x, y, z) offset, yaw, pitch, roll, scale,
    anim + anim_time (skeletal meshes), attach_to (component name on part 0, relative identity like
    TE_LightRadixiteArmor does), hide / show (component-name substrings).
    Batch 2 additions (all optional): child_actors {ChildActorComponent name substring: BP path} fills empty
    child actor slots on the spawned temp actor (squad unit weapons are assigned at runtime);
    attach_socket + rel_loc / rel_rot (roll, pitch, yaw) for attach_to; materials [(slot, path)] overrides on the
    temp components only.
    kwargs: res, azimuth, elevation, fov, padding, exposure_bias, light_intensity, ambient_intensity,
    ambient_cubemap, debug (see _capture_staged).
    """
    spawned = []
    try:
        actors, comps = _stage_parts(parts, spawned)
        return _capture_staged(out_name, actors, comps, spawned, **kwargs)
    finally:
        if not keep:
            _destroy(spawned)


def render_unit(asset, out_name, unit_yaw=-60.0, hide=(), **kwargs):
    """Single BP / mesh shortcut for render_scene."""
    return render_scene(out_name, [dict(asset=asset, yaw=unit_yaw, hide=hide)], **kwargs)
