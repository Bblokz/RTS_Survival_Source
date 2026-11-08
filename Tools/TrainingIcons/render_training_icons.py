"""Training-icon renders for the squads without a DT_TrainingOptions picture (Training icons agent, 2026-11-05).

The live squad training icons (T_Ger_Squad_*, sources D:/UE_Assets/Icons/TrainingUI/GerSquads/T_Ger_Portrait_*) are
in-world portraits: one or two soldiers seen from chest height in a dark desert at dusk, a blue role glyph bottom left.
This script stages the squads' own soldier Blueprints (weapon child actors and kit included) on the ground of a real map
and renders full-scene captures (landscape + sky, no show-only list), plus a coverage mask per shot for compositing.

Never run this in the shared editor. It runs in its own DebugGame editor process under
Saved/AgentLocks/debuggame_build.lockdir (run_training_icons.ps1), waits for shaders, renders every job twice with texture
streaming off (the first capture after an asset loads can be texture-starved), writes renders/done.txt and quits.

TRAINICONS_MODE=explore : background plates around the anchor (8 yaws x exposure brackets x GI on/off) + a test pair.
TRAINICONS_MODE=final   : the JOBS below (optionally TRAINICONS_JOBS=a,b subset).
"""
import json
import math
import os
import time

import unreal

exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read())

RENDER_DIR = r"D:/UE5Projects/RTS_Survival/Saved/TrainingIcons/renders"
MODE = os.environ.get("TRAINICONS_MODE", "explore")
WANTED = [n.strip() for n in os.environ.get("TRAINICONS_JOBS", "").split(",") if n.strip()]
SQ = "/Game/RTS_Survival/Blueprints/Squads/"
ANIMS = SQ + "SquadUnits/Anims/"
STAND_AIM = ANIMS + "AimPosMontages/BaseAnimations/Crouch_to_Stand_Rifle_Ironsights"
CROUCH_AIM = ANIMS + "AimPosMontages/BaseAnimations/Stand_to_Crouch_Rifle_Ironsights"
IDLE_HIP = ANIMS + "Idle/Idle_Rifle_Hip"
ELITE_STG = SQ + "Ger/EliteInfantry/EliteScavengers/Ger_Inf_EliteScavenger_STG"
ELITE_FAUST = SQ + "Ger/EliteInfantry/EliteScavengers/Ger_Inf_EliteScavenger_Faust"
RED_SPINE = SQ + "Ger/Walkers/RedSpine/Ger_Inf_Walker_RedSpine"
FELS_KRIEGER = SQ + "Ger/Walkers/FelsKrieger/Ger_Inf_Walker_FelsKrieger"
RAIL_TROOPER = SQ + "Ger/EliteInfantry/RailTroopers/Ger_Inf_RailTrooper"
HIDE = ("Selection", "Decal")
LOG = "[TrainingIcons] "

# Ground point: the map's BP_UnitImage_LandscapeAnchor (the unit portrait spot) unless TRAINICONS_GROUND="x,y".
ANCHOR_PREFIX = "BP_UnitImage_LandscapeAnchor"


def _env_json(name, default):
    raw = os.environ.get(name, "")
    return json.loads(raw) if raw else default


def find_ground_origin():
    override = os.environ.get("TRAINICONS_GROUND", "")
    if override:
        x, y = (float(v) for v in override.split(","))
        return ground_at(x, y), 0.0
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if actor.get_class().get_name().startswith(ANCHOR_PREFIX):
            loc = actor.get_actor_location()
            unreal.log(LOG + "anchor %s at %s yaw %.1f" % (actor.get_name(), loc, actor.get_actor_rotation().yaw))
            return ground_at(loc.x, loc.y), actor.get_actor_rotation().yaw
    unreal.log_warning(LOG + "no anchor; using 0,0")
    return ground_at(0.0, 0.0), 0.0


def ground_at(x, y):
    world = _world()
    start, end = unreal.Vector(x, y, 50000.0), unreal.Vector(x, y, -50000.0)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return unreal.Vector(x, y, 0.0)
    loc = hit.to_tuple()[4]  # Location
    return unreal.Vector(loc.x, loc.y, loc.z)


def full_scene_capture(world, res, cam_loc, cam_rot, fov, exposure_bias, lumen, show_only_actors=None,
                       source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR, hdr=False):
    """hdr=True: linear float target (exported as EXR); scene captures here ignore the exposure bias, so the
    composer exposes and tonemaps the HDR picture itself."""
    cap_actor = _actor_sub().spawn_actor_from_class(unreal.SceneCapture2D, cam_loc, cam_rot)
    cap = cap_actor.capture_component2d
    fmt = unreal.TextureRenderTargetFormat.RTF_RGBA16F if hdr else unreal.TextureRenderTargetFormat.RTF_RGBA8
    rt = unreal.RenderingLibrary.create_render_target2d(world, res, res, fmt, unreal.LinearColor(0, 0, 0, 0))
    cap.set_editor_property("texture_target", rt)
    cap.set_editor_property("capture_source", source)
    if show_only_actors is not None:
        cap.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        for a in show_only_actors:
            cap.show_only_actor_components(a, False)
    cap.set_editor_property("fov_angle", fov)
    cap.set_editor_property("capture_every_frame", False)
    cap.set_editor_property("capture_on_movement", False)
    flags = [unreal.EngineShowFlagsSetting(show_flag_name=f, enabled=False)
             for f in ("MotionBlur", "Grain", "LensFlares")]
    cap.set_editor_property("show_flag_settings", flags)
    pp = cap.get_editor_property("post_process_settings")
    pp.set_editor_property("override_auto_exposure_method", True)
    pp.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    pp.set_editor_property("override_auto_exposure_bias", True)
    pp.set_editor_property("auto_exposure_bias", exposure_bias)
    pp.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
    pp.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
    gi = unreal.DynamicGlobalIlluminationMethod.LUMEN if lumen else unreal.DynamicGlobalIlluminationMethod.NONE
    refl = unreal.ReflectionMethod.LUMEN if lumen else unreal.ReflectionMethod.SCREEN_SPACE
    pp.set_editor_property("override_dynamic_global_illumination_method", True)
    pp.set_editor_property("dynamic_global_illumination_method", gi)
    pp.set_editor_property("override_reflection_method", True)
    pp.set_editor_property("reflection_method", refl)
    cap.set_editor_property("post_process_settings", pp)
    cap.set_editor_property("post_process_blend_weight", 1.0)
    return cap_actor, cap, rt


def camera_pose(origin, azimuth, dist, height, look_height):
    az = math.radians(azimuth)
    cam_loc = origin + unreal.Vector(math.cos(az) * dist, math.sin(az) * dist, height)
    target = origin + unreal.Vector(0.0, 0.0, look_height)
    return cam_loc, unreal.MathLibrary.find_look_at_rotation(cam_loc, target)


def export(world, rt, name, ext=".png"):
    unreal.RenderingLibrary.export_render_target(world, rt, RENDER_DIR, name + ext)


def stage_soldiers(origin, cam_rot, soldiers, spawned):
    """soldier keys: bp, x (towards camera, cm), y (screen right, cm), face (deg; 0 = faces the camera, + turns the
    soldier towards screen right), anim, frac (0..1 of the animation), scale (extra, on top of the BP's mesh scale)."""
    fwd = cam_rot.get_forward_vector()
    right = cam_rot.get_right_vector()
    fwd_flat = unreal.Vector(fwd.x, fwd.y, 0.0).normal()
    right_flat = unreal.Vector(right.x, right.y, 0.0).normal()
    cam_yaw = cam_rot.yaw
    actors, comps = [], []
    poses = []
    for s in soldiers:
        xy = origin - fwd_flat * s.get("x", 0.0) + right_flat * s.get("y", 0.0)
        ground = ground_at(xy.x, xy.y)
        yaw = cam_yaw + 180.0 - s.get("face", 0.0)
        cls = unreal.EditorAssetLibrary.load_blueprint_class(s["bp"])
        actor = _actor_sub().spawn_actor_from_class(cls, ground + unreal.Vector(0, 0, 200.0), unreal.Rotator(0, 0, yaw))
        spawned.append(actor)
        capsule = actor.get_component_by_class(unreal.CapsuleComponent)
        half = capsule.get_scaled_capsule_half_height() if capsule else 90.0
        actor.set_actor_location(ground + unreal.Vector(0, 0, half + s.get("z", 0.0)), False, False)
        if s.get("anim"):
            comp = _pose_component(actor)
            anim = unreal.load_asset(s["anim"])
            target = comp
            if s.get("leader_mesh"):
                # The mesh's skeleton has no animations (SK_RailTrooper is on SK_PostApoSoldier_Skeleton): pose a
                # hidden same-bone proxy on InfantryMaster_Skeleton and let the soldier follow it (leader pose).
                proxy = _actor_sub().spawn_actor_from_class(unreal.SkeletalMeshActor, comp.get_world_location(),
                                                            comp.get_world_rotation())
                spawned.append(proxy)
                target = proxy.skeletal_mesh_component
                target.set_editor_property("visibility_based_anim_tick_option",
                                           unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
                target.set_update_animation_in_editor(True)
                target.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
                target.override_animation_data(anim, False, False, anim.get_play_length() * s.get("frac", 0.5), 0.0)
                target.set_skinned_asset_and_update(unreal.load_asset(s["leader_mesh"]), True)
                target.set_world_scale3d(comp.get_world_scale())
            target.set_update_animation_in_editor(True)
            target.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            target.set_animation(anim)
            target.set_position(anim.get_play_length() * s.get("frac", 0.5), False)
            target.set_play_rate(0.0)
            poses.append((target, anim, anim.get_play_length() * s.get("frac", 0.5), comp, actor))
            if target is not comp:
                before = comp.get_socket_location("hand_r")
                comp.set_leader_pose_component(target, True)
                target.set_position(anim.get_play_length() * s.get("frac", 0.5), False)
                unreal.log(LOG + "leader pose hand_r follower %s -> %s, leader %s" % (
                    before, comp.get_socket_location("hand_r"), target.get_socket_location("hand_r")))
                target.set_visibility(False, False)
        part_actors = _collect_actors(actor)
        for a in part_actors:
            for c in a.get_components_by_class(unreal.SceneComponent):
                if isinstance(c, (unreal.DecalComponent, unreal.WidgetComponent)) or any(
                        h.lower() in c.get_name().lower() for h in HIDE):
                    c.set_visibility(False, False)
        actors.extend(part_actors)
        comps.extend(_visible_mesh_components(part_actors, HIDE))
        weapon_info = [(a.get_class().get_name(), str(a.get_attach_parent_socket_name()) if hasattr(
            a, "get_attach_parent_socket_name") else "?") for a in part_actors[1:]]
        unreal.log(LOG + "soldier %s yaw %.0f half %.0f attached %s" % (s["bp"].split("/")[-1], yaw, half, weapon_info))
    # Pose everything again once all soldiers exist (the first soldier's pose was evaluated before its animation
    # finished loading), then re-attach the attached actors (weapons) so they follow leader-posed hands.
    for target, anim, position, comp, actor in poses:
        target.set_position(position, False)
        if target is not comp:
            comp.set_leader_pose_component(target, True)
        # components on the posed mesh (the Weapon child actor component sits on a hand socket)
        for c in actor.get_components_by_class(unreal.SceneComponent):
            if c != comp and c.get_attach_parent() == comp:
                c.attach_to_component(comp, c.get_attach_socket_name(), unreal.AttachmentRule.KEEP_RELATIVE,
                                      unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE, False)
                unreal.log(LOG + "reattached component %s socket %s" % (c.get_name(), c.get_attach_socket_name()))
        for child in actor.get_attached_actors():
            socket = child.get_attach_parent_socket_name()
            parent = child.root_component.get_attach_parent()
            if parent is None:
                continue
            child.root_component.attach_to_component(parent, socket, unreal.AttachmentRule.KEEP_RELATIVE,
                                                     unreal.AttachmentRule.KEEP_RELATIVE,
                                                     unreal.AttachmentRule.KEEP_RELATIVE, False)
            unreal.log(LOG + "reattached %s to %s socket %s" % (child.get_class().get_name(), parent.get_name(), socket))
    return actors, comps


def fit_camera(comps, cam_rot, job):
    """Frames the group like the live portraits: heads near the top, the frame covering job['cover'] of the group's
    height from the top (0.75 = knees up), centred on the group; same view direction, slight downward look."""
    lo = [1e18] * 3
    hi = [-1e18] * 3
    for c in comps:
        origin, extent, _ = unreal.SystemLibrary.get_component_bounds(c)
        for i, (o, e) in enumerate(((origin.x, extent.x), (origin.y, extent.y), (origin.z, extent.z))):
            lo[i] = min(lo[i], o - e)
            hi[i] = max(hi[i], o + e)
    top, bottom = hi[2], lo[2]
    top = max(top, bottom + job.get("min_group_h", 0.0))
    frame_h = (top - bottom) * job["cover"] / (1.0 - job.get("head_margin", 0.08))
    frame_top = top + frame_h * job.get("head_margin", 0.08)
    center = unreal.Vector((lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5, frame_top - frame_h * 0.5)
    fwd = cam_rot.get_forward_vector()
    fwd_flat = unreal.Vector(fwd.x, fwd.y, 0.0).normal()
    right_flat = unreal.Vector(cam_rot.get_right_vector().x, cam_rot.get_right_vector().y, 0.0).normal()
    center = center + right_flat * (job.get("shift", 0.0) * frame_h)
    dist = frame_h * 0.5 / math.tan(math.radians(job["fov"]) * 0.5)
    cam_loc = center - fwd_flat * dist + unreal.Vector(0.0, 0.0, frame_h * job.get("drop", 0.12))
    rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, center)
    unreal.log(LOG + "fit group h %.0f frame %.0f dist %.0f" % (top - bottom, frame_h, dist))
    return cam_loc, rot


def render_job(name, job, origin, base_yaw, label):
    world = _world()
    spawned = []
    try:
        cam_loc, cam_rot = camera_pose(origin, base_yaw + job["azimuth"], job["dist"], job["height"], job["look"])
        actors, comps = stage_soldiers(origin, cam_rot, job["soldiers"], spawned)
        if job.get("cover"):
            cam_loc, cam_rot = fit_camera(comps, cam_rot, job)
        for c in comps:
            c.prestream_textures(60.0, True)
        for bias in job.get("biases", [job.get("bias", 0.0)]):
            for lumen in job.get("lumen", [True]):
                cap_actor, cap, rt = full_scene_capture(world, job.get("res", 1024), cam_loc, cam_rot, job["fov"], bias,
                                                        lumen)
                spawned.append(cap_actor)
                cap.capture_scene()
                export(world, rt, "%s_b%+.1f_%s" % (name, bias, "lumen" if lumen else "nogi"))
        for lumen in job.get("lumen", [True]):
            for src_name, src in (("finalhdr", unreal.SceneCaptureSource.SCS_FINAL_COLOR_HDR),
                                  ("scenehdr", unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)):
                cap_actor, cap, rt = full_scene_capture(world, job.get("res", 1024), cam_loc, cam_rot, job["fov"], 0.0,
                                                        lumen, None, src, hdr=True)
                spawned.append(cap_actor)
                cap.capture_scene()
                export(world, rt, "%s_%s_%s" % (name, src_name, "lumen" if lumen else "nogi"), ".exr")
        # coverage mask (alpha = 1 - coverage on the HDR pass), same camera, soldiers only
        cap_actor, cap, rt = full_scene_capture(world, job.get("res", 1024), cam_loc, cam_rot, job["fov"], 0.0, False,
                                                actors, unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)
        spawned.append(cap_actor)
        cap.capture_scene()
        export(world, rt, name + "_mask")
        unreal.log(LOG + "%s rendered %s cam %s rot %s" % (label, name, cam_loc, cam_rot))
    finally:
        _destroy(spawned)


def render_plates(origin, base_yaw, label):
    world = _world()
    spawned = []
    try:
        for i in PLATE_STEPS:
            az = base_yaw + i * 45.0
            cam_loc, cam_rot = camera_pose(origin, az, 400.0, 150.0, 110.0)
            for bias in PLATE_BIASES:
                for lumen in (True, False):
                    cap_actor, cap, rt = full_scene_capture(world, 384, cam_loc, cam_rot, 30.0, bias, lumen)
                    spawned.append(cap_actor)
                    cap.capture_scene()
                    export(world, rt, "plate_%s_az%03d_b%+.1f_%s" % (label, i * 45, bias, "lumen" if lumen else "nogi"))
            cap_actor, cap, rt = full_scene_capture(world, 384, cam_loc, cam_rot, 30.0, 0.0, True, None,
                                                    unreal.SceneCaptureSource.SCS_FINAL_COLOR_HDR, hdr=True)
            spawned.append(cap_actor)
            cap.capture_scene()
            export(world, rt, "plate_%s_az%03d_finalhdr" % (label, i * 45), ".exr")
        unreal.log(LOG + "%s plates done" % label)
    finally:
        _destroy(spawned)


PLATE_STEPS = _env_json("TRAINICONS_PLATE_STEPS", [0, 1, 7])
PLATE_BIASES = _env_json("TRAINICONS_PLATE_BIASES", [0.0, 2.0, 4.0])
TEST_PAIR = dict(azimuth=0.0, dist=420.0, height=150.0, look=115.0, fov=30.0, res=512, biases=_env_json("TRAINICONS_PLATE_BIASES", [0.0, 2.0, 4.0]),
                 lumen=[True, False],
                 soldiers=[dict(bp=ELITE_STG, x=0.0, y=-40.0, face=55.0, anim=STAND_AIM, frac=0.95),
                           dict(bp=ELITE_FAUST, x=-120.0, y=70.0, face=35.0, anim=IDLE_HIP, frac=0.3)])

# Final shots: azimuth relative to the anchor yaw (chosen from the explore plates), exposure from the explore brackets.
FINAL_VIEW = _env_json("TRAINICONS_VIEW", dict(azimuth=0.0, dist=420.0, height=150.0, look=115.0, fov=30.0, res=1024,
                                               biases=[0.0], lumen=[True]))
POSTAPO = SQ + "SquadUnits/Skeletons/PostApocalyptic/Mesh/SK_PostApoSoldier"
# Measured: CROUCH_AIM near its end renders a standing ironsights aim, STAND_AIM near its end a crouched aim.
JOBS = {
    "EliteScavengers": dict(soldiers=[dict(bp=ELITE_STG, x=0.0, y=0.0, face=50.0, anim=STAND_AIM, frac=0.95),
                                      dict(bp=ELITE_FAUST, x=-130.0, y=110.0, face=30.0, anim=CROUCH_AIM, frac=0.98)],
                            min_group_h=195.0, cover=0.86),
    "RedSpine": dict(soldiers=[dict(bp=RED_SPINE, x=0.0, y=20.0, face=50.0, anim=STAND_AIM, frac=0.95),
                               dict(bp=RED_SPINE, x=-230.0, y=170.0, face=25.0, anim=STAND_AIM, frac=0.95)],
                     dist=760.0, height=240.0, look=200.0),
    "FelsKrieger": dict(soldiers=[dict(bp=FELS_KRIEGER, x=0.0, y=0.0, face=40.0, anim=CROUCH_AIM, frac=0.98)],
                        dist=900.0, height=280.0, look=230.0, cover=0.84, head_margin=0.12),
    # SK_RailTrooper is on SK_PostApoSoldier_Skeleton while AnimBp_SquadUnit and every infantry animation are on
    # InfantryMaster_Skeleton (no compatible-skeleton link), so the troopers show their reference pose (rifle held
    # upright at the side) in game too; leader-posing them from SK_PostApoSoldier warps the body (bind axes differ).
    "RailTroopers": dict(soldiers=[dict(bp=RAIL_TROOPER, x=0.0, y=0.0, face=45.0)],
                         min_group_h=200.0, cover=0.8, shift=-0.1),
}
JOBS.update(_env_json("TRAINICONS_JOB_OVERRIDES", {}))

SCHEDULE = [(float(os.environ.get("TRAINICONS_FIRST", "150")), "first"),
            (float(os.environ.get("TRAINICONS_SECOND", "240")), "second"), (0.0, "quit")]
_state = {"start": time.time(), "step": 0, "handle": None, "log": []}


# A windowless editor starts on low scalability: no eye adaptation (exposure bias ignored) and no Lumen.
QUALITY_COMMANDS = ("scalability 3", "r.EyeAdaptationQuality 2", "r.Lumen.DiffuseIndirect.Allow 1")
SKY_FILL = float(os.environ.get("TRAINICONS_SKYFILL", "0"))


def spawn_sky_fill():
    """Movable sky light capturing the map's sky: lifts the black shadows like UnitPortraitCaptureSubsystem's fill."""
    if SKY_FILL <= 0.0:
        return None
    light = _actor_sub().spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    comp = light.light_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
    comp.set_intensity(SKY_FILL)
    comp.recapture_sky()
    return light


def _run_pass(label):
    unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
    for command in QUALITY_COMMANDS:
        unreal.SystemLibrary.execute_console_command(None, command)
    sky = None
    try:
        sky = None if (MODE == "explore" and label == "first") else spawn_sky_fill()
        origin, base_yaw = find_ground_origin()
        _state["log"].append("%s origin %s yaw %.1f" % (label, origin, base_yaw))
        if MODE == "explore":
            tag = "fill" if sky else "nofill"
            render_plates(origin, base_yaw, tag)
            render_job("test_pair_" + tag, TEST_PAIR, origin, base_yaw, label)
            _state["log"].append("%s ok explore" % label)
            return
        for name in WANTED or list(JOBS):
            job = dict(FINAL_VIEW)
            job.update(JOBS[name])
            try:
                render_job(name, job, origin, base_yaw, label)
                _state["log"].append("%s ok %s" % (label, name))
            except Exception as error:
                _state["log"].append("%s FAIL %s: %s" % (label, name, error))
    except Exception as error:
        _state["log"].append("%s FAIL pass: %s" % (label, error))
    finally:
        if sky:
            _actor_sub().destroy_actor(sky)
        unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 1")


def _tick(delta_seconds):
    if _state["step"] >= len(SCHEDULE):
        return
    at, what = SCHEDULE[_state["step"]]
    if what != "quit" and time.time() - _state["start"] < at:
        return
    _state["step"] += 1
    if what == "quit":
        with open(RENDER_DIR + "/done.txt", "w") as done:
            done.write("\n".join(_state["log"]))
        unreal.unregister_slate_post_tick_callback(_state["handle"])
        unreal.SystemLibrary.quit_editor()
        return
    _run_pass(what)
    unreal.log(LOG + "%s pass done" % what)


os.makedirs(RENDER_DIR, exist_ok=True)
_state["handle"] = unreal.register_slate_post_tick_callback(_tick)
