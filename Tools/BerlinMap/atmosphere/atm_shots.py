"""Look tests for the Berlin atmosphere in MY throwaway editor (atm_editor.sh). Never saves the level.

    import atm_shots as s; s.apply(); _defer = s.sequence([...])

A step is (file name, camera target (x, y) or None to keep the camera, callable or None run before the shot, seconds to wait).
The camera stands like the game camera: 28 m above the ground, pitched down 50 degrees, looking north.
"""
import importlib
import json
import math
import sys
import time

import unreal

FOLDER = "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/atmosphere"
if FOLDER not in sys.path:
    sys.path.insert(0, FOLDER)
import atmosphere_place     # noqa: E402

OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/atmosphere/shots/"
CAMERA_HEIGHT, CAMERA_PITCH, CAMERA_FOV = 2800.0, -50.0, 85.0
SUB = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def placement():
    with open(atmosphere_place.PLACEMENT_FILE, "r") as handle:
        return json.load(handle)


def ground_z(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    try:
        return hit.to_tuple()[4].z if hit is not None else 0.0
    except Exception:  # noqa: BLE001
        return 0.0


def apply():
    importlib.reload(atmosphere_place)
    return atmosphere_place.run()


def ours():
    """(actors, components on hosts) of the atmosphere."""
    actors, components = [], []
    for actor in SUB.get_all_level_actors():
        if actor.get_actor_label().startswith(atmosphere_place.PREFIX):
            actors.append(actor)
        else:
            components += [c for c in actor.get_components_by_tag(unreal.ActorComponent, atmosphere_place.TAG)]
    return actors, components


def show(on, only=None):
    """Shows or hides the whole layer, or only the actors whose label contains one of the given words."""
    actors, components = ours()
    for actor in actors:
        visible = on and (only is None or any(word in actor.get_actor_label() for word in only))
        actor.set_is_temporarily_hidden_in_editor(not visible)
        for comp in actor.get_components_by_class(unreal.SceneComponent):
            comp.set_visibility(visible, False)
    for comp in components:
        visible = on and (only is None or any(word in comp.get_name() for word in only))
        comp.set_visibility(visible, False)


def set_knob(name, value):
    """Unsaved change of a knob's default (this editor never saves): the render picks it up at once."""
    collection = unreal.load_asset(atmosphere_place.ROOT + "MPC_BerlinAtmosphere")
    params = collection.get_editor_property("scalar_parameters")
    for index, parameter in enumerate(params):
        if str(parameter.get_editor_property("parameter_name")) == name:
            parameter.set_editor_property("default_value", value)
            params[index] = parameter
    collection.set_editor_property("scalar_parameters", params)
    collection.post_edit_change()


def camera_for(x, y, height=CAMERA_HEIGHT, pitch=CAMERA_PITCH, yaw=0.0):
    z = ground_z(x, y)
    back = height / math.tan(math.radians(-pitch))
    return (unreal.Vector(x - back * math.cos(math.radians(yaw)), y - back * math.sin(math.radians(yaw)), z + height),
            unreal.Rotator(0.0, pitch, yaw))


def sequence(steps, settle=7.0):
    state = {"i": 0, "t": time.time(), "cam": None, "wait": 1.0}

    def tick():
        if time.time() - state["t"] < state["wait"]:
            return False
        if state.get("pending"):
            name = state.pop("pending")
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, OUT + name + ".png", state["cam"])
            state["t"], state["wait"] = time.time(), 1.5
            return False
        if state["i"] >= len(steps):
            if state["cam"] is not None:
                SUB.destroy_actor(state["cam"])
                state["cam"] = None
            return True
        name, target, before, wait = steps[state["i"]]
        state["i"] += 1
        if before:
            before()
        if target is not None:
            if state["cam"] is not None:
                SUB.destroy_actor(state["cam"])
            height = target[2] if len(target) > 2 else CAMERA_HEIGHT
            pitch = target[3] if len(target) > 3 else CAMERA_PITCH
            yaw = target[4] if len(target) > 4 else 0.0
            location, rotation = camera_for(target[0], target[1], height, pitch, yaw)
            cam = SUB.spawn_actor_from_class(unreal.CameraActor, location, rotation)
            cam.camera_component.set_field_of_view(CAMERA_FOV)
            cam.camera_component.set_editor_property("constrain_aspect_ratio", False)
            state["cam"] = cam
            # the level viewport follows, so the Niagara distance culling and the streaming see the same place
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(location, rotation)
        state["pending"] = name
        state["t"], state["wait"] = time.time(), (settle if wait is None else wait)
        return False

    return tick


# ---- the same in a running game (PIE in the throwaway editor): Niagara only simulates there -------------------------------
SHOT_GAP = 1.2      # seconds after a shot before the next step; lower it for bursts (flicker)
TEST_CAMERA = "ATMX_TestCamera"     # not ATM_: clear() of the build step must not count it


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def game_actors():
    return unreal.GameplayStatics.get_all_actors_of_class(game_world(), unreal.Actor)


def game_ours():
    actors, components = [], []
    for actor in game_actors():
        if actor.get_actor_label().startswith(atmosphere_place.PREFIX):
            actors.append(actor)
        else:
            components += [c for c in actor.get_components_by_tag(unreal.ActorComponent, atmosphere_place.TAG)]
    return actors, components


def game_show(on, only=None):
    actors, components = game_ours()
    for actor in actors:
        visible = on and (only is None or any(word in actor.get_actor_label() for word in only))
        # the actor flag only: a Niagara component whose own visibility was switched off and on again came back
        # without its flames and smoke (the fires looked dead in the first test rounds because of that)
        actor.set_actor_hidden_in_game(not visible)
        for comp in actor.get_components_by_class(unreal.LightComponent):
            comp.set_visibility(visible, False)
    for comp in components:
        visible = on and (only is None or any(word in comp.get_name() for word in only))
        comp.set_visibility(visible, False)
    return len(actors), len(components)


def game_ground_z(x, y):
    hit = unreal.SystemLibrary.line_trace_single(game_world(), unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    try:
        return hit.to_tuple()[4].z if hit is not None else 0.0
    except Exception:  # noqa: BLE001
        return 0.0


def console(command):
    unreal.SystemLibrary.execute_console_command(game_world(), command)


def pie_sequence(steps, settle=6.0, pp_off=True, after=None, keep_running=False):
    """Starts PIE, runs the steps in the game world, ends PIE. A step is (name or None, target or None, callable or None,
    seconds). target = (x, y[, height, pitch, yaw]). With pp_off the fog of war post process is off (a map without units
    is all black under it)."""
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les.is_in_play_in_editor() and not [a for a in SUB.get_all_level_actors() if a.get_actor_label() == TEST_CAMERA]:
        cam = SUB.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 3000))
        cam.set_actor_label(TEST_CAMERA)
    state = {"phase": "start", "t": time.time(), "i": 0, "wait": 0.5, "cam": None}

    def tick():
        now = time.time()
        if state["phase"] == "start":
            if not les.is_in_play_in_editor():
                les.editor_request_begin_play()
            state["phase"], state["t"] = "wait_pie", now
            return False
        if state["phase"] == "wait_pie":
            if game_world() is None or now - state["t"] < 14.0:
                if now - state["t"] > 240.0:
                    raise RuntimeError("PIE never started")
                return False
            world = game_world()
            unreal.GameplayStatics.set_game_paused(world, False)
            for command in ("t.MaxFPS 0", "r.VSync 0") + (("ShowFlag.PostProcessMaterial 0",) if pp_off else ()):
                console(command)
            cams = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor) if a.get_actor_label() == TEST_CAMERA]
            state["cam"] = cams[0]
            cam = state["cam"]
            cam.camera_component.set_editor_property("constrain_aspect_ratio", False)
            cam.camera_component.set_field_of_view(CAMERA_FOV)
            cam.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            print("game world", world.get_path_name(), "ours", [len(v) for v in game_ours()])
            state["phase"], state["t"], state["wait"] = "steps", now, 1.0
            return False
        if state["phase"] == "steps":
            if now - state["t"] < state["wait"]:
                return False
            if state.get("pending"):
                name = state.pop("pending")
                if pp_off:
                    console("ShowFlag.PostProcessMaterial 0")
                unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, OUT + name + ".png", state["cam"])
                state["t"], state["wait"] = now, SHOT_GAP
                return False
            if state["i"] >= len(steps):
                if after:
                    after()
                if keep_running:
                    return True
                les.editor_request_end_play()
                state["phase"], state["t"] = "ending", now
                return False
            name, target, before, wait = steps[state["i"]]
            state["i"] += 1
            if before:
                result = before()
                if result is not None:
                    print(name, "->", result)
            if target is not None:
                height = target[2] if len(target) > 2 else CAMERA_HEIGHT
                pitch = target[3] if len(target) > 3 else CAMERA_PITCH
                yaw = target[4] if len(target) > 4 else 0.0
                z = game_ground_z(target[0], target[1])
                back = height / math.tan(math.radians(-pitch))
                location = unreal.Vector(target[0] - back * math.cos(math.radians(yaw)), target[1] - back * math.sin(math.radians(yaw)), z + height)
                state["cam"].set_actor_location(location, False, True)
                state["cam"].set_actor_rotation(unreal.Rotator(0.0, pitch, yaw), True)
                unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(state["cam"], 0.0)
            if name:
                state["pending"] = name
            state["t"], state["wait"] = now, (settle if wait is None else wait)
            return False
        if state["phase"] == "ending":
            return (not les.is_in_play_in_editor()) and now - state["t"] > 3.0
        return True

    def safe_tick():
        try:
            return tick()
        except Exception:
            if les.is_in_play_in_editor():
                les.editor_request_end_play()       # never leave the game running after a failed step
            raise

    return safe_tick


# ---- cost: frame time medians and ProfileGPU dumps, with the layer shown and hidden -----------------------------------------
BENCH = {"collect": False, "frames": [], "results": [], "handle": None, "start": 0.0}


def _bench_tick(delta):
    if BENCH["collect"] and time.time() >= BENCH["start"]:
        BENCH["frames"].append(delta)


def bench_begin(label, on, settle=1.5):
    """Shows or hides the layer and starts collecting frame times after the settle time. Call bench_end() in the next step."""
    if BENCH["handle"] is None:
        BENCH["handle"] = unreal.register_slate_post_tick_callback(_bench_tick)
    game_show(on)
    BENCH["label"], BENCH["frames"], BENCH["start"], BENCH["collect"] = label, [], time.time() + settle, True


def bench_end(profile=True):
    BENCH["collect"] = False
    frames = sorted(BENCH["frames"])
    if not frames:
        return None
    entry = dict(state=BENCH["label"], frames=len(frames), median_ms=round(frames[len(frames) // 2] * 1000.0, 3),
                 p10_ms=round(frames[len(frames) // 10] * 1000.0, 3), p90_ms=round(frames[len(frames) * 9 // 10] * 1000.0, 3))
    BENCH["results"].append(entry)
    if profile:
        unreal.log("ATM_PROFILE_BEGIN " + BENCH["label"])
        console("ProfileGPU")
    return entry


def rendered_counts():
    """What of the layer the renderer drew in the last 0.3 s: sheet cards, sprite systems, lights."""
    actors, _ = game_ours()
    out = dict(sheets=0, systems=0, lights=0)
    for actor in actors:
        for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
            if comp.was_recently_rendered(0.3):
                key = "sheets" if isinstance(comp, unreal.StaticMeshComponent) else "systems"
                out[key] += 1
        if isinstance(actor, unreal.PointLight):
            out["lights"] += 1 if (actor.get_actor_location() - unreal.GameplayStatics.get_player_camera_manager(game_world(), 0).get_camera_location()).length() < 9000.0 else 0
    return out
