"""Runtime look check of the supply truck: PIE in a throwaway DebugGame editor with -SupplyPointValidate running.
Follows the first BP_Ger_OpelSupplyTruck with scene captures and logs wheel-bone rotation against its velocity.
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject -SupplyPointValidate -SupplyPointAt=16796,-15808
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="r.TextureStreaming 0, py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
The map test ends the process itself (RequestExit); a watchdog quits after 5 minutes.
"""
import math
import traceback
import unreal

OUT = "D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/TruckBP"
S = {"t": 0.0, "phase": "wait_load", "mark": 0.0, "shots": 0, "samples": 0, "next_shot": 0.0, "last": None}
# (name suffix, offset in truck space, look-at height)
VIEWS = (("drive_front_three_quarter", (900.0, -700.0, 420.0)), ("drive_rear_three_quarter", (-900.0, 650.0, 450.0)),
         ("drive_side", (0.0, -1000.0, 160.0)), ("drive_rts_camera", (-900.0, 0.0, 1500.0)))


def log(message):
    unreal.log("OPELDRIVE " + message)


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def find_truck(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
        if actor.get_class().get_name() == "BP_Ger_OpelSupplyTruck_C":
            return actor
    return None


def camera_pose(truck, offset):
    eye = truck.get_actor_transform().transform_location(unreal.Vector(*offset))
    target = truck.get_actor_location() + unreal.Vector(0.0, 0.0, 120.0)
    return eye, unreal.MathLibrary.find_look_at_rotation(eye, target)


def follow(world, truck, offset):
    """Keeps the capture on the truck every frame, so its eye adaptation settles before a picture is exported."""
    # Placed in the editor level before PIE (python cannot spawn into the game world), so it is static until made movable.
    actor = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SceneCapture2D)[0]
    cap = actor.capture_component2d
    if S.get("target") is None:
        actor.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        S["target"] = unreal.RenderingLibrary.create_render_target2d(
            world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        cap.set_editor_property("texture_target", S["target"])
        cap.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        cap.set_editor_property("fov_angle", 40.0)
        cap.set_editor_property("capture_every_frame", True)
        cap.set_editor_property("always_persist_rendering_state", True)
        # Wheel spin is only refreshed on rendered meshes by default; the game viewport looks elsewhere.
        truck.root_component.set_editor_property(
            "visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    eye, rotation = camera_pose(truck, offset)
    actor.set_actor_location(eye, False, True)
    actor.set_actor_rotation(rotation, True)
    # RTSOptimizer stops skeletal ticks outside the player's view, so the player camera has to look at the truck too
    # (the RTS camera pawn takes the view target back between frames).
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(actor, 0.0)


def capture(world, name):
    unreal.RenderingLibrary.export_render_target(world, S["target"], OUT, name + ".png")
    log(f"picture {name}")


def play_rate(track):
    try:
        return round(track.get_anim_instance().get_editor_property("PlayRate"), 2)
    except Exception:
        return "?"


# Truck-local rays through the cab, the bed boards, from behind, from the front, down into the bed, and one above it.
HIT_RAYS = {"cab_side": ((150, -600, 170), (150, 600, 170)), "bed_boards_side": ((-150, -600, 150), (-150, 600, 150)),
            "rear": ((-900, 0, 150), (900, 0, 150)), "front": ((900, 0, 110), (-900, 0, 110)),
            "top_bed": ((-150, 0, 900), (-150, 0, -100)), "above_truck_miss": ((-900, 0, 400), (900, 0, 400))}


def hit_check(world, truck):
    """Weapons trace the hidden Hull (ArmorCalculation's mesh): prove it takes hits at runtime."""
    transform = truck.get_actor_transform()
    hull = [c for c in truck.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "Hull"][0]
    log(f"hull collision={hull.get_collision_enabled()} object={hull.get_collision_object_type()} "
        f"anim_rate_scale={truck.root_component.global_anim_rate_scale}")
    for name, (start, end) in HIT_RAYS.items():
        world_start = transform.transform_location(unreal.Vector(*start))
        world_end = transform.transform_location(unreal.Vector(*end))
        result = hull.line_trace_component(world_start, world_end, False, False, False)
        local = transform.inverse_transform_location(result[0]) if result else None
        first = unreal.SystemLibrary.line_trace_single(world, world_start, world_end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                       False, [], unreal.DrawDebugTrace.NONE, True)
        first_comp = first.to_tuple()[10].get_name() if first and first.to_tuple()[10] else None
        log(f"hit_ray {name} hull_hit_local={'(%.0f,%.0f,%.0f)' % (local.x, local.y, local.z) if local else None} "
            f"visibility_first_component={first_comp}")


def sample(truck):
    track = truck.root_component
    velocity = truck.get_velocity()
    forward = truck.get_actor_forward_vector()
    speed_forward = velocity.dot(forward)
    hull = [c for c in truck.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "Hull"][0]
    # Wheel spin shows in the bone's rotation relative to the truck.
    relative = {}
    for bone in ("FL", "FR", "RL", "RR"):
        bone_transform = track.get_socket_transform(bone, unreal.RelativeTransformSpace.RTS_COMPONENT)
        rot = bone_transform.rotation.rotator()
        relative[bone] = (round(rot.pitch, 1), round(rot.yaw, 1), round(rot.roll, 1))
    location = truck.get_actor_location()
    ground = unreal.SystemLibrary.line_trace_single(
        truck.get_world(), location + unreal.Vector(0, 0, 500), location - unreal.Vector(0, 0, 2000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [truck], unreal.DrawDebugTrace.NONE, True)
    ground_z = ground.to_tuple()[4].z if ground else float("nan")
    log(f"sample speed_forward={speed_forward:.0f} speed={velocity.length():.0f} origin_above_ground={location.z - ground_z:.1f} "
        f"pitch={truck.get_actor_rotation().pitch:.1f} roll={truck.get_actor_rotation().roll:.1f} wheels={relative} "
        f"hull_visible={hull.is_visible()} hull_hidden_in_game={hull.hidden_in_game} "
        f"wheel_anim={track.get_anim_instance().get_editor_property('CurrentWheelAnimation')} play_rate={play_rate(track)}")
    return speed_forward


def tick(delta):
    S["t"] += delta
    try:
        if S["t"] > 300.0:
            log("watchdog quit")
            unreal.SystemLibrary.quit_editor()
            return
        if S["phase"] == "wait_load" and S["t"] > 15.0:
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
                unreal.SceneCapture2D, unreal.Vector(16796.0, -15808.0, 3000.0))
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S["phase"], S["mark"] = "pie", S["t"]
            log("pie requested")
        elif S["phase"] == "pie":
            world = game_world()
            if world is None:
                return
            truck = find_truck(world)
            if truck is None:
                S["target"] = None
                return
            view = S["shots"] % len(VIEWS)
            follow(world, truck, VIEWS[view][1])
            if S["t"] < S["next_shot"]:
                return
            S["next_shot"] = S["t"] + 0.5
            speed = sample(truck)
            if S["samples"] == 6:
                hit_check(world, truck)
            S["samples"] += 1
            # Three samples (1.5 s) on a view before its picture: exposure and streaming settle.
            if speed > 300.0 and S["shots"] < 12 and S["samples"] % 3 == 0:
                capture(world, f"{VIEWS[view][0]}_{S['shots'] // len(VIEWS) + 1}")
                S["shots"] += 1
    except Exception:
        log("ERROR " + traceback.format_exc())
        S["phase"] = "failed"
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(tick)
log("script loaded")
