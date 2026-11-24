"""Leave-and-return test of the street litter's distance cull, old system against new, in a throwaway editor on
RTS_ToCopySmall (never Ger_Berlin, never the shared editor; nothing is saved). Launch: ambient_cull_run.sh <logname>.

Two litter stretches lie side by side on empty ground, 30 m apart, both running along +X:
  OLD  y = -1500  NS_BerlinStreetLitter    (effect type NET_BerlinAmbient: deactivate and resume; wisp material reads alpha)
  NEW  y = +1500  NS_BerlinStreetLitter_B  (effect type NET_BerlinAmbient_Pause: pause and resume; wisp reads R)
The game camera (28 m up, -70 degrees) looks at the point between them, goes 400 m away (cull distance 95 m) and comes
back. At every stop: is_active of both components and pictures. Saved/Berlin/ambient/cull_test/: visit1_*, away,
visit2_* (15 s after the return), visit3_* (30 s after), result.json. In a picture the OLD stretch is the upper half
(the camera looks along +X, so -Y is on its left... see result.json "old_side").
"""
import json
import math
import os

import unreal

ROOT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/cull_test/"
LITTER = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Ambient/Litter/"
SYSTEMS = {"old": (LITTER + "NS_BerlinStreetLitter", -1500.0), "new": (LITTER + "NS_BerlinStreetLitter_B", 1500.0)}
CANDIDATES = [(-9000.0, 0.0), (9000.0, 0.0), (0.0, 9000.0), (0.0, -9000.0), (-9000.0, -9000.0), (9000.0, 9000.0)]
FAR_OFFSET = 40000.0        # cm: the camera's "away" stop, far past the 95 m cull distance
# (phase name, seconds to wait in it, camera away?, pictures to take when the wait is over)
PLAN = [("visit1", 16.0, False, ("visit1_0", "visit1_a", "visit1_b")), ("away", 10.0, True, ("away",)),
        ("visit2", 15.0, False, ("visit2_0", "visit2_a", "visit2_b")), ("visit3", 15.0, False, ("visit3_0", "visit3_a", "visit3_b"))]
S = {"phase": "wait_load", "t": 0.0, "mark": 0.0, "log": [], "result": {"old_side": "left half of the picture (-Y)"}, "step": 0, "shots": []}


def log(message):
    S["log"].append("%.1f %s" % (S["t"], message))
    unreal.log("AMBCULL " + str(message))


def finish():
    try:
        json.dump(dict(log=S["log"], result=S["result"]), open(ROOT + "result.json", "w"), indent=1, default=str)
    finally:
        S["phase"] = "done"
        unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    try:
        return hit.to_tuple()[4].z if hit is not None else 0.0
    except Exception:  # noqa: BLE001
        return 0.0


def place():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # a spot on the landscape with nothing of the test map within 60 m
    others = [a.get_actor_location() for a in actors.get_all_level_actors() if a.get_class().get_name() not in ("Landscape", "LandscapeStreamingProxy")]
    centre = None
    for cx, cy in CANDIDATES:
        on_ground = all(abs(ground_z(world, cx + dx, cy + dy)) > 0.01 for dx in (-2500.0, 0.0, 2500.0) for dy in (-2500.0, 0.0, 2500.0))
        crowd = sum(1 for o in others if math.hypot(o.x - cx, o.y - cy) < 6000.0)
        log("candidate %.0f %.0f: on landscape %s, actors within 60 m %d" % (cx, cy, on_ground, crowd))
        if on_ground and crowd == 0 and centre is None:
            centre = (cx, cy)
    if centre is None:
        centre = CANDIDATES[0]
    S["centre"] = centre
    S["result"]["centre"] = centre
    CENTRE_X = centre[0]
    for key, (path, offset) in SYSTEMS.items():
        y = centre[1] + offset
        system = unreal.load_asset(path)
        actor = actors.spawn_actor_from_object(system, unreal.Vector(CENTRE_X, y, ground_z(world, CENTRE_X, y)), unreal.Rotator(0.0, 0.0, 0.0))
        actor.set_actor_label("CULL_" + key)
        effect_type = system.get_editor_property("effect_type")
        log("%s: %s, effect type %s, cull reaction %s" % (key, system.get_name(), effect_type.get_name(), effect_type.get_editor_property("cull_reaction")))
        S["result"][key + "_setup"] = dict(system=system.get_name(), effect_type=effect_type.get_name(),
                                           cull_reaction=str(effect_type.get_editor_property("cull_reaction")))
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(CENTRE_X, centre[1], 3000.0)).set_actor_label("CULL_Camera")


def find_runtime(world):
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S["camera"] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(S["centre"][0], S["centre"][1], 3000.0)).length())
    S["camera"].camera_component.set_editor_property("constrain_aspect_ratio", False)
    S["camera"].camera_component.set_field_of_view(90.0)
    S["comps"] = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NiagaraActor):
        for key, (_path, offset) in SYSTEMS.items():
            location = actor.get_actor_location()
            if abs(location.x - S["centre"][0]) < 10.0 and abs(location.y - S["centre"][1] - offset) < 10.0:
                S["comps"][key] = actor.get_component_by_class(unreal.NiagaraComponent)
    log("runtime components %s" % sorted(S["comps"]))


def camera_at(away):
    x, y = S["centre"][0] + (FAR_OFFSET if away else 0.0), S["centre"][1]
    z = ground_z(game_world(), S["centre"][0], y)
    eye = unreal.Vector(x - 2800.0 / math.tan(math.radians(70.0)), y, z + 2800.0)
    camera = S["camera"]
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, unreal.Vector(x, y, z)), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def state():
    return {key: dict(active=bool(comp.is_active()), recently_rendered=bool(comp.was_recently_rendered(0.3))) for key, comp in S["comps"].items()}


def picture(name):
    S["busy"] = True
    try:
        unreal.SystemLibrary.execute_console_command(game_world(), "ShowFlag.PostProcessMaterial 0")
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, "%s%s.png" % (ROOT, name), camera=S["camera"])
    finally:
        S["busy"] = False


def tick(delta):
    S["t"] += delta
    if S.get("busy") or S["phase"] == "done":
        return
    try:
        if S["phase"] == "wait_load" and S["t"] > 15.0:
            os.makedirs(ROOT, exist_ok=True)
            S["phase"], S["busy"] = "placing", True
            try:
                place()
            finally:
                S["busy"] = False
            S["phase"], S["mark"] = "placed", S["t"]
        elif S["phase"] == "placed" and S["t"] - S["mark"] > 4.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S["phase"], S["mark"] = "wait_pie", S["t"]
        elif S["phase"] == "wait_pie" and game_world() is not None and S["t"] - S["mark"] > 14.0:
            unreal.GameplayStatics.set_game_paused(game_world(), False)
            unreal.SystemLibrary.execute_console_command(game_world(), "ShowFlag.PostProcessMaterial 0")
            find_runtime(game_world())
            camera_at(PLAN[0][2])
            S["phase"], S["mark"] = "plan", S["t"]
        elif S["phase"] == "plan":
            name, wait, _x, shots = PLAN[S["step"]]
            if S["t"] - S["mark"] < wait:
                return
            if not S["shots"] and not S.get("shot_done"):
                S["result"][name] = state()
                log("%s after %.0f s: %s" % (name, wait, S["result"][name]))
                S["shots"] = list(shots)
            if S["shots"]:
                if S["t"] - S.get("shot_mark", 0.0) < 1.0:
                    return
                picture(S["shots"].pop(0))
                S["shot_mark"], S["shot_done"] = S["t"], True
                return
            if S["t"] - S.get("shot_mark", 0.0) < 1.0:
                return
            S["shot_done"] = False
            S["step"] += 1
            if S["step"] >= len(PLAN):
                finish()
                return
            camera_at(PLAN[S["step"]][2])
            S["mark"] = S["t"]
    except Exception as error:  # noqa: BLE001
        import traceback
        log("EXCEPTION %s\n%s" % (error, traceback.format_exc()))
        finish()


unreal.register_slate_post_tick_callback(tick)
