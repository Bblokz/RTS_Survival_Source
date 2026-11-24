"""Throwaway-editor test of the Berlin ambient motion on RTS_ToCopySmall (never Ger_Berlin, never the shared editor).
Launch with ambient_run.sh <logname> [timeout_s]. The map is never saved; the editor quits at the end.

It copies a slice of the real map to the test map: every ruin of the dress file within SLICE_RADIUS of a hero ruin that
has a crow group, as real BP_BerlinRuin actors labelled RUIN_<piece>_<index>, moved to the test map's origin. Then it runs
the SAME placement code the map build runs (ambient_place.place_litter / place_dressing / place_crows / place_flocks) with
the real Ger_Berlin_ambient.json entries of those ruins, starts PIE and:
  1. takes pictures: three frames a second apart from an RTS view (30 m up, -70 degrees) and from closer views;
  2. measures: frame time and draw calls with the ambient parts shown / hidden (four off/on pairs), material instruction
     counts, Niagara instances awake near and far from the camera;
  3. kills one dressed ruin and checks its dressing and crows are gone with it.
Results: Saved/Berlin/ambient/test/result.json + PNGs.
"""
import json
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ambient")
sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ambient")
import ambient_gen      # noqa: E402
import ambient_place    # noqa: E402

ROOT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/test/"
RUIN_BP = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/BP_BerlinRuin_"
SLICE_RADIUS = 7500.0
PICTURE_HOLD = 2.0
S = {"phase": "wait_load", "t": 0.0, "mark": 0.0, "log": [], "result": {}, "pics": [], "frames": [], "bench": []}


def log(message):
    S["log"].append("%.1f %s" % (S["t"], message))
    unreal.log("AMBTEST " + str(message))


def finish():
    try:
        os.makedirs(ROOT, exist_ok=True)
        json.dump(dict(log=S["log"], result=S["result"]), open(ROOT + "result.json", "w"), indent=1, default=str)
    finally:
        S["phase"] = "done"
        unreal.SystemLibrary.quit_editor()


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    try:
        return hit.to_tuple()[4].z if hit is not None else 0.0
    except Exception:  # noqa: BLE001
        return 0.0


# ---- the slice ---------------------------------------------------------------------------------------------------------
def place_slice():
    world = editor_world()
    log("editor map %s" % world.get_path_name())
    data = json.load(open(ambient_place.AMBIENT_FILE))
    ruins = json.load(open(ambient_place.DRESS_FILE))["ruins"]
    log("stamp ok: %s" % (ambient_gen.ruin_stamp(ruins) == data["stamp"]))
    dressed = {}
    for item in data["dressing"]:
        dressed[item[0]] = dressed.get(item[0], 0) + 1
    crow_ruins = {g["ruin"] for g in data["crow_groups"]}

    def score(index):
        r = ruins[index]
        near = [i for i, o in enumerate(ruins) if math.hypot(o["x"] - r["x"], o["y"] - r["y"]) < SLICE_RADIUS]
        litter = sum(1 for l in data["litter"] if math.hypot(l[0] - r["x"], l[1] - r["y"]) < SLICE_RADIUS)
        return sum(dressed.get(i, 0) for i in near) + 6 * sum(1 for i in near if i in crow_ruins) + 10 * min(litter, 3)

    heroes = [i for i in crow_ruins if ruins[i]["piece"].split("_")[0] in ("E1", "E2", "E3", "B3", "B5")]
    centre_index = max(heroes or list(crow_ruins), key=score)
    centre = ruins[centre_index]
    cx, cy = centre["x"], centre["y"]
    log("slice centre ruin %d %s at %.0f %.0f" % (centre_index, centre["piece"], cx, cy))
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    lookup = {}
    for index, ruin in enumerate(ruins):
        if math.hypot(ruin["x"] - cx, ruin["y"] - cy) > SLICE_RADIUS:
            continue
        cls = unreal.EditorAssetLibrary.load_blueprint_class(RUIN_BP + ruin["piece"])
        x, y = ruin["x"] - cx, ruin["y"] - cy
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, ground_z(world, x, y)), unreal.Rotator(0.0, 0.0, ruin["yaw"]))
        actor.set_actor_label("RUIN_%s_%04d" % (ruin["piece"], index))
        comp = actor.get_component_by_class(unreal.StaticMeshComponent)
        comp.set_default_custom_primitive_data_vector4(0, unreal.Vector4(ruin["tint"][0], ruin["tint"][1], ruin["tint"][2], ruin["soot"]))
        lookup[index] = actor
    S["lookup_labels"] = {i: a.get_actor_label() for i, a in lookup.items()}
    litter = []
    for x, y, _z, yaw, width in data["litter"]:
        if math.hypot(x - cx, y - cy) < SLICE_RADIUS + 2000.0:
            litter.append([x - cx, y - cy, ground_z(world, x - cx, y - cy), yaw, width])
    # two more stretches far away: they must be asleep while the camera is on the slice
    litter.append([30000.0, 0.0, ground_z(world, 30000.0, 0.0), 0.0, 1.0])
    litter.append([-30000.0, 0.0, ground_z(world, -30000.0, 0.0), 0.0, 1.0])
    placed = dict(
        litter=ambient_place.place_litter(litter),
        dressing=ambient_place.place_dressing([i for i in data["dressing"] if i[0] in lookup], lookup),
        crows=ambient_place.place_crows([g for g in data["crow_groups"] if g["ruin"] in lookup], lookup),
        flocks=ambient_place.place_flocks([dict(data["crow_flocks"][0], x=0.0, y=0.0, z=ground_z(world, 0.0, 0.0), semi_x=3500.0, semi_y=2200.0)]))
    # the clear step must find and remove all of it again, and a second placement must give the same counts
    removed = ambient_place.clear(lookup)
    again = dict(
        litter=ambient_place.place_litter(litter),
        dressing=ambient_place.place_dressing([i for i in data["dressing"] if i[0] in lookup], lookup),
        crows=ambient_place.place_crows([g for g in data["crow_groups"] if g["ruin"] in lookup], lookup),
        flocks=ambient_place.place_flocks([dict(data["crow_flocks"][0], x=0.0, y=0.0, z=ground_z(world, 0.0, 0.0), semi_x=3500.0, semi_y=2200.0)]))
    log("placed %s; cleared %s; placed again %s" % (placed, removed, again))
    S["result"]["slice"] = dict(ruins=len(lookup), placed=placed, cleared=removed, placed_again=again, centre=[cx, cy],
                                centre_piece=centre["piece"])
    # targets for the close views
    S["targets"] = {}
    kinds = {"curtain": "Curtain", "wire": "WireSpan", "cable": "CableDangle", "shutter": "Shutter", "lamp": "Lamp"}
    for key, token in kinds.items():
        for index, actor in lookup.items():
            comps = [c for c in actor.get_components_by_class(unreal.StaticMeshComponent)
                     if ambient_place.is_ambient(c) and c.static_mesh is not None and token in c.static_mesh.get_name()]
            if comps:
                S["targets"][key] = (index, comps[0].get_world_location(), comps[0].get_forward_vector())
                break
    for index, actor in lookup.items():
        comps = [c for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent) if ambient_place.is_ambient(c)]
        if comps:
            transform = comps[0].get_instance_transform(0, True)
            transform = transform[1] if isinstance(transform, tuple) else transform
            location = transform.translation if transform is not None else comps[0].get_world_location()
            S["targets"]["crows"] = (index, location, unreal.Vector(1, 0, 0))
            break
    log("targets %s" % {k: (v[0], [round(v[1].x), round(v[1].y), round(v[1].z)]) for k, v in S["targets"].items()})
    names = sorted(c.get_name() for a in lookup.values() for c in a.get_components_by_class(unreal.ActorComponent) if ambient_place.is_ambient(c))
    S["result"]["slice"]["component_names_after_replacing"] = names
    log("ambient components after clear + place: %d, unnamed %d" % (len(names), sum(1 for n in names if not n.startswith("AMB_"))))
    actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, 3000))
    # 40 copies of one dressing mesh, set up like the placed components, 300 m from the slice (see the "copies" phase)
    curtain = unreal.load_asset(ambient_place.DRESSING + "SM_BerlinAmb_Curtain_A")
    for index in range(40):
        x, y = 30000.0 + (index % 8) * 150.0, 300.0 + (index // 8) * 300.0
        copy = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, ground_z(world, x, y) + 300.0), unreal.Rotator(0.0, 0.0, 180.0))
        copy.set_actor_label("AMBX_Copy_%02d" % index)
        comp = copy.static_mesh_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_static_mesh(curtain)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("cast_shadow", False)
        comp.set_visibility(False, False)
    material_stats()


def material_stats():
    stats = {}
    paths = {
        "MI_BerlinAmb_Cloth": ambient_place.DRESSING + "MI_BerlinAmb_Cloth", "MI_BerlinAmb_Wire": ambient_place.DRESSING + "MI_BerlinAmb_Wire",
        "MI_BerlinAmb_Rigid": ambient_place.DRESSING + "MI_BerlinAmb_Rigid", "M_BerlinAmbient_Crow": ambient_place.DRESSING + "M_BerlinAmbient_Crow",
        "M_BerlinAmbient_Litter": ambient_place.ROOT + "Litter/M_BerlinAmbient_Litter", "M_BerlinAmbient_WispPuff": ambient_place.ROOT + "Litter/M_BerlinAmbient_WispPuff",
        "compare MI_BF_Trim_Flutter": "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/Motion/MI_BF_Trim_Flutter",
        "compare MI_Crow_VAT": "/Game/RTS_Survival/Blueprints/Environment/Animated/Birds/DesertFlocks/Crow/MI_Crow_VAT",
        "compare MI_BerlinRuin_Wall": "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/MI_BerlinRuin_Wall",
    }
    for name, path in paths.items():
        asset = unreal.load_asset(path)
        if asset is None:
            continue
        try:
            st = unreal.MaterialEditingLibrary.get_statistics(asset)
            stats[name] = dict(vs=st.num_vertex_shader_instructions, ps=st.num_pixel_shader_instructions, samplers=st.num_samplers,
                               vt=st.num_vertex_texture_samples, pt=st.num_pixel_texture_samples)
        except Exception as error:  # noqa: BLE001
            stats[name] = str(error)
    S["result"]["material_instructions"] = stats
    log("material stats %s" % stats)


# ---- runtime ----------------------------------------------------------------------------------------------------------
def find_runtime(world):
    cams = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor)
    S["camera"] = min(cams, key=lambda c: (c.get_actor_location() - unreal.Vector(0, 0, 3000)).length())
    S["camera"].camera_component.set_editor_property("constrain_aspect_ratio", False)
    S["camera"].camera_component.set_field_of_view(90.0)
    ruins = {}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name().startswith("BP_BerlinRuin_"):
            ruins[actor.get_actor_label()] = actor
    S["ruins"] = ruins
    S["niagara"] = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NiagaraActor)]
    S["flocks"] = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.BirdFlock)]
    S["copies"] = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)
                   if a.static_mesh_component.static_mesh is not None and a.static_mesh_component.static_mesh.get_name() == "SM_BerlinAmb_Curtain_A"]
    dressing, crows = [], []
    for actor in ruins.values():
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if not ambient_place.is_ambient(comp):
                continue
            if isinstance(comp, unreal.InstancedStaticMeshComponent):
                crows.append(comp)
            else:
                dressing.append(comp)
    S["dressing"], S["crows"] = dressing, crows
    log("runtime: ruins %d, niagara actors %d, flocks %d, dressing components %d, crow components %d (%d birds)" % (
        len(ruins), len(S["niagara"]), len(S["flocks"]), len(dressing), len(crows), sum(c.get_instance_count() for c in crows)))
    S["result"]["runtime"] = dict(ruins=len(ruins), niagara=len(S["niagara"]), flocks=len(S["flocks"]), dressing=len(dressing),
                                  crow_components=len(crows), crow_birds=sum(c.get_instance_count() for c in crows))


def place_camera(eye, look):
    camera = S["camera"]
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(eye, False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, look), True)
    unreal.GameplayStatics.get_player_controller(game_world(), 0).set_view_target_with_blend(camera, 0.0)


def rts_view(target, height=3000.0):
    """The game camera's framing: 30 m up, pitched -70 degrees."""
    return unreal.Vector(target.x - height / math.tan(math.radians(70.0)), target.y, target.z + height), target


def close_view(key, dist, height, side=0.35):
    _index, location, forward = S["targets"][key]
    right = unreal.Vector(-forward.y, forward.x, 0.0)
    eye = location + forward * dist + right * (dist * side) + unreal.Vector(0, 0, height)
    return eye, location + unreal.Vector(0, 0, -60.0)


def set_ambient(litter, dressing, crows):
    for actor in S["niagara"]:
        comp = actor.get_component_by_class(unreal.NiagaraComponent)
        comp.set_visibility(litter, False)
        comp.set_active(litter, False)
    for comp in S["dressing"]:
        comp.set_visibility(dressing, False)
    for comp in S["crows"]:
        comp.set_visibility(crows, False)
    for flock in S["flocks"]:
        flock.set_actor_hidden_in_game(not crows)
        flock.set_actor_tick_enabled(crows)


def awake_litter():
    awake = 0
    for actor in S["niagara"]:
        comp = actor.get_component_by_class(unreal.NiagaraComponent)
        try:
            awake += 1 if comp.is_active() else 0
        except Exception:  # noqa: BLE001
            pass
    return awake


def picture(name):
    S["busy"] = True
    try:
        unreal.SystemLibrary.execute_console_command(game_world(), "ShowFlag.PostProcessMaterial 0")
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, "%s%s.png" % (ROOT, name), camera=S["camera"])
    finally:
        S["busy"] = False
    log("picture %s" % name)


def queue_views():
    centre = unreal.Vector(0.0, 0.0, ground_z(game_world(), 0.0, 0.0))
    views = [("rts_centre", rts_view(centre))]
    lane = S["niagara"][0].get_actor_location() if S["niagara"] else centre
    views.append(("rts_lane", rts_view(lane)))
    views.append(("lane_low", (lane + unreal.Vector(-900.0, -500.0, 700.0), lane + unreal.Vector(600.0, 0.0, 60.0))))
    for key, dist, height in (("curtain", 900.0, 250.0), ("wire", 1400.0, 500.0), ("cable", 800.0, 150.0), ("shutter", 700.0, 150.0),
                              ("lamp", 700.0, 150.0), ("crows", 600.0, 350.0)):
        if key in S["targets"]:
            views.append(("close_" + key, close_view(key, dist, height)))
            views.append(("rts_" + key, rts_view(S["targets"][key][1] + unreal.Vector(0, 0, -S["targets"][key][1].z + centre.z))))
    for name, view in views:
        # a screenshot can land a moment after it is asked for: frame 0 only settles the camera and is not kept for judging
        for frame in "0abc":
            S["pics"].append(("%s_%s" % (name, frame), view, frame == "0"))


def run_pictures(next_phase):
    """First frame of a view: move, wait PICTURE_HOLD, shoot. Following frames: shoot 1 s apart without moving."""
    if S.get("pending"):
        name, wait = S["pending"]
        if S["t"] - S["mark"] < wait:
            return True
        S.pop("pending")
        S["mark"] = S["t"]
        picture(name)
        return True
    if S["t"] - S["mark"] < 0.25:
        return True
    if S["pics"]:
        name, (eye, look), move = S["pics"].pop(0)
        if move:
            place_camera(eye, look)
        S["pending"], S["mark"] = (name, PICTURE_HOLD if move else 1.0), S["t"]
        return True
    S["phase"], S["mark"] = next_phase, S["t"]
    return False


# ---- the bench: off/on pairs of frame time, draw calls from the stat dump ------------------------------------------------
BENCH_STATES = [("all_off", (False, False, False)), ("all_on", (True, True, True))] * 4 + \
               [("only_litter", (True, False, False)), ("only_dressing", (False, True, False)), ("only_crows", (False, False, True)),
                ("all_off", (False, False, False)), ("all_on", (True, True, True))]
BENCH_SETTLE, BENCH_FRAMES = 2.5, 240


def bench(delta):
    b = S.setdefault("bench_state", {"index": -1, "frames": [], "mark": S["t"], "stage": "next"})
    if b["stage"] == "next":
        b["index"] += 1
        if b["index"] >= len(BENCH_STATES):
            return False
        name, flags = BENCH_STATES[b["index"]]
        set_ambient(*flags)
        b["frames"], b["mark"], b["stage"] = [], S["t"], "settle"
        return True
    if b["stage"] == "settle":
        if S["t"] - b["mark"] >= BENCH_SETTLE:
            b["stage"] = "measure"
        return True
    b["frames"].append(delta)
    if len(b["frames"]) >= BENCH_FRAMES:
        frames = sorted(b["frames"])
        name = BENCH_STATES[b["index"]][0]
        entry = dict(state=name, median_ms=round(frames[len(frames) // 2] * 1000.0, 3), mean_ms=round(sum(frames) / len(frames) * 1000.0, 3),
                     awake_litter=awake_litter())
        S["bench"].append(entry)
        log("bench %s" % entry)
        b["stage"] = "next"
    return True


def tick(delta):
    S["t"] += delta
    if S.get("busy") or S["phase"] == "done":
        return
    phase = S["phase"]
    try:
        if phase == "wait_load" and S["t"] > 15.0:
            os.makedirs(ROOT, exist_ok=True)
            S["phase"] = "placing"          # shader compiles inside pump the Slate tick: never re-enter
            S["busy"] = True
            try:
                place_slice()
            finally:
                S["busy"] = False
            S["phase"], S["mark"] = "placed", S["t"]
        elif phase == "placed" and S["t"] - S["mark"] > 4.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S["phase"], S["mark"] = "wait_pie", S["t"]
        elif phase == "wait_pie":
            world = game_world()
            if world is not None and S["t"] - S["mark"] > 14.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                for command in ("ShowFlag.PostProcessMaterial 0", "t.MaxFPS 0", "r.VSync 0"):
                    unreal.SystemLibrary.execute_console_command(world, command)
                find_runtime(world)
                queue_views()
                picture("_warmup")
                S["phase"], S["mark"] = "pictures", S["t"]
            elif S["t"] - S["mark"] > 180.0:
                log("pie never started")
                finish()
        elif phase == "pictures":
            run_pictures("bench_setup")
        elif phase == "bench_setup":
            centre = unreal.Vector(0.0, 0.0, ground_z(game_world(), 0.0, 0.0))
            place_camera(*rts_view(centre))
            S["result"]["awake_litter_at_slice"] = None
            S["phase"], S["mark"] = "bench", S["t"]
        elif phase == "bench":
            if S["t"] - S["mark"] < 3.0:
                return
            if S["result"].get("awake_litter_at_slice") is None:
                S["result"]["awake_litter_at_slice"] = dict(awake=awake_litter(), of=len(S["niagara"]))
                log("litter awake with the camera on the slice: %s" % S["result"]["awake_litter_at_slice"])
            if not bench(delta):
                S["result"]["bench"] = S["bench"]
                on = [e["median_ms"] for e in S["bench"][:8] if e["state"] == "all_on"]
                off = [e["median_ms"] for e in S["bench"][:8] if e["state"] == "all_off"]
                deltas = sorted(a - b for a, b in zip(on, off))
                S["result"]["bench_pairs_on_minus_off_ms"] = deltas
                log("bench pair deltas (on - off, ms): %s" % deltas)
                set_ambient(True, True, True)
                busiest = max(S["ruins"].values(), key=lambda r: sum(1 for c in S["dressing"] + S["crows"] if c.get_owner() == r))
                place_camera(*rts_view(busiest.get_actor_location()))
                S["phase"], S["mark"] = "rendered", S["t"]
        elif phase == "rendered" and S["t"] - S["mark"] > 2.5:
            first = S["dressing"][0] if S["dressing"] else None
            S["result"]["dressing_component_settings"] = None if first is None else dict(
                cast_shadow=first.get_editor_property("cast_shadow"), wpo_disable_distance=first.get_editor_property("world_position_offset_disable_distance"),
                max_draw_distance=first.get_editor_property("ld_max_draw_distance"), mobility=str(first.mobility),
                collision=str(first.get_collision_enabled()), tick=first.is_component_tick_enabled())
            seen = dict(dressing=sum(1 for c in S["dressing"] if c.was_recently_rendered(0.3)),
                        crow_components=sum(1 for c in S["crows"] if c.was_recently_rendered(0.3)),
                        litter_components=sum(1 for a in S["niagara"] if a.get_component_by_class(unreal.NiagaraComponent).was_recently_rendered(0.3)))
            S["result"]["rendered_in_busiest_rts_view"] = seen
            log("dressing component settings in the game: %s; rendered in the busiest RTS view: %s" % (S["result"]["dressing_component_settings"], seen))
            place_camera(*close_view("crows", 600.0, 350.0))
            S["phase"], S["mark"] = "airborne_before", S["t"]
        elif phase == "airborne_before" and S["t"] - S["mark"] > 2.5:
            # the crow manager contract: per-instance custom data 2 = 1 folds a perched bird away
            picture("airborne_0_all_perched")
            owner = S["ruins"].get(S["lookup_labels"][S["targets"]["crows"][0]])
            S["airborne_comp"] = next(c for c in S["crows"] if c.get_owner() == owner)
            for instance in (0, 1):
                S["airborne_comp"].set_custom_data_value(instance, 2, 1.0, True)
            S["phase"], S["mark"] = "airborne_after", S["t"]
        elif phase == "airborne_after" and S["t"] - S["mark"] > 1.5:
            picture("airborne_1_two_folded")
            for instance in (0, 1):
                S["airborne_comp"].set_custom_data_value(instance, 2, 0.0, True)
            S["phase"], S["mark"] = "flock_find", S["t"]
        elif phase == "flock_find" and S["t"] - S["mark"] > 0.5:
            flock = S["flocks"][0]
            birds = flock.get_component_by_class(unreal.InstancedStaticMeshComponent)
            heights = []
            for instance in range(birds.get_instance_count()):
                transform = birds.get_instance_transform(instance, True)
                transform = transform[1] if isinstance(transform, tuple) else transform
                heights.append(round(transform.translation.z - ground_z(game_world(), transform.translation.x, transform.translation.y)))
                if instance == 0:
                    lead = transform.translation
            S["result"]["flock_bird_heights_cm"] = heights
            log("flock bird heights above the ground: %s" % heights)
            place_camera(*rts_view(unreal.Vector(lead.x, lead.y, ground_z(game_world(), lead.x, lead.y))))
            S["phase"], S["mark"], S["flock_shots"] = "flock_shot", S["t"], 0
        elif phase == "flock_shot" and S["t"] - S["mark"] > 0.9:
            picture("flock_rts_%s" % "abc"[S["flock_shots"]])
            S["flock_shots"] += 1
            S["mark"] = S["t"]
            if S["flock_shots"] >= 3:
                centre = unreal.Vector(30000.0 + 525.0, 900.0, ground_z(game_world(), 30500.0, 900.0))
                place_camera(*rts_view(centre))
                S["phase"], S["copies_index"] = "copies", 0
        elif phase == "copies":
            # 40 copies of one curtain mesh, alone in the view: do equal dressing components merge into one draw call?
            if S["t"] - S["mark"] < 1.2:
                return
            shown = S["copies_index"] % 2 == 1
            unreal.log("AMBTEST COPIES %s" % ("shown" if shown else "hidden"))
            unreal.SystemLibrary.execute_console_command(game_world(), "r.MeshDrawCommands.LogDynamicInstancingStats 1")
            S["copies_index"] += 1
            S["mark"] = S["t"]
            for actor in S["copies"]:
                actor.static_mesh_component.set_visibility(S["copies_index"] % 2 == 1, False)
            if S["copies_index"] >= 8:
                # far away: every stretch of the slice must go to sleep, the far one near the camera wakes
                place_camera(unreal.Vector(30000.0 - 1090.0, 0.0, 3000.0), unreal.Vector(30000.0, 0.0, 0.0))
                S["phase"], S["mark"] = "far", S["t"]
        elif phase == "far" and S["t"] - S["mark"] > 6.0:
            S["result"]["awake_litter_camera_300m_away"] = dict(awake=awake_litter(), of=len(S["niagara"]))
            log("litter awake with the camera 300 m away: %s" % S["result"]["awake_litter_camera_300m_away"])
            # death: the dressed ruin of the curtain close-up (or the crow ruin) dies; its components must go with it
            key = "crows" if "crows" in S["targets"] else "curtain"
            index = S["targets"][key][0]
            label = S["lookup_labels"][index]
            victim = S["ruins"].get(label)
            S["victim"] = victim
            S["victim_comps"] = [c for c in S["dressing"] + S["crows"] if c.get_owner() == victim]
            log("killing %s with %d ambient components" % (label, len(S["victim_comps"])))
            location = victim.get_actor_location()
            place_camera(location + unreal.Vector(-1800.0, -900.0, 2200.0), location + unreal.Vector(0, 0, 500.0))
            S["phase"], S["mark"] = "kill", S["t"]
        elif phase == "kill" and S["t"] - S["mark"] > 2.5:
            picture("death_before")
            unreal.GameplayStatics.apply_damage(S["victim"], 9999999.0, None, None, None)
            S["phase"], S["mark"] = "death_1", S["t"]
        elif phase == "death_1" and S["t"] - S["mark"] > 1.0:
            picture("death_1s")
            visible = sum(1 for c in S["victim_comps"] if unreal.SystemLibrary.is_valid(c) and c.is_visible())
            S["result"]["death"] = dict(components=len(S["victim_comps"]), still_visible_after_1s=visible)
            S["phase"], S["mark"] = "death_2", S["t"]
        elif phase == "death_2" and S["t"] - S["mark"] > 7.0:
            picture("death_8s")
            alive = sum(1 for c in S["victim_comps"] if unreal.SystemLibrary.is_valid(c))
            S["result"]["death"]["alive_after_8s"] = alive
            S["result"]["death"]["actor_alive_after_8s"] = bool(unreal.SystemLibrary.is_valid(S["victim"]))
            log("death: %s" % S["result"]["death"])
            S["phase"], S["mark"] = "end", S["t"]
        elif phase == "end" and S["t"] - S["mark"] > 2.0:
            finish()
    except Exception as error:  # noqa: BLE001
        import traceback
        log("EXCEPTION in %s: %s\n%s" % (phase, error, traceback.format_exc()))
        finish()


unreal.register_slate_post_tick_callback(tick)
