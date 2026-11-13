"""Game-run check of the Berlin atmosphere layers against fog of war, unit readability and selection decals.
Run in MY throwaway editor started on a unit-test map that has player units (atm_editor.sh /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall),
with atm_cmd.py. Never saves. Shots: Saved/Berlin/atmosphere/shots/game_*.png, taken through the REAL game camera with the
fog of war post process ON.

Editor phase: puts every kind of layer on the units of the test map: two haze sheets (an unsaved material instance lays
Mitte's part of the district mask over the test map), Mitte smoke, the crater system (heat shimmer, vapour), a smouldering
shell and a fire with its light. Game phase: moves the player's camera pawn there, shoots with the layer shown and hidden,
selects a unit, then moves to the edge of the units' sight.
"""
import json
import statistics
import time

import unreal

import atm_shots as s
import atmosphere_place as p

OUT = s.OUT
actors = s.SUB.get_all_level_actors()
pawns = [a for a in actors if isinstance(a, unreal.Pawn)]
classes = {}
for a in pawns:
    classes[a.get_class().get_name()] = classes.get(a.get_class().get_name(), 0) + 1
print("pawn classes", classes)
# RTS_ToCopySmall spawns its units at game start: the player riflemen stand round this point (read from a first run)
cx, cy = -20100.0, -8500.0
cz = 140.0      # the ground there (a trace from above hits the map's camera bounds first)
print("centre", cx, cy, cz)
data = s.placement()
rect = data["mask"]["rect"]
mx, my = data["views"]["mitte"]
tools = unreal.AssetToolsHelpers.get_asset_tools()
path = "/Game/Developers/AtmTest/MI_AtmTest_Sheet"
instance = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(
    "MI_AtmTest_Sheet", "/Game/Developers/AtmTest", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
MEL = unreal.MaterialEditingLibrary
MEL.set_material_instance_parent(instance, unreal.load_asset(p.ROOT + "M_BerlinAtmo_Sheet"))
MEL.set_material_instance_vector_parameter_value(instance, "MaskRect", unreal.LinearColor(rect[0] + cx - mx, rect[2] + cy - my, rect[1] - rect[0], rect[3] - rect[2]))
MEL.update_material_instance(instance)
for a in actors:
    if a.get_actor_label().startswith(p.PREFIX):
        s.SUB.destroy_actor(a)
plane = unreal.load_asset(p.PLANE)
for level, height in enumerate(data["sheets"]["heights"]):
    actor = s.SUB.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(cx, cy, cz + height), unreal.Rotator(0, 0, 0))
    actor.set_actor_scale3d(unreal.Vector(80.0, 80.0, 1.0))
    actor.set_actor_label("ATM_Sheet_T%d" % level)
    comp = actor.static_mesh_component
    comp.set_static_mesh(plane)
    comp.set_material(0, instance)
    p.quiet(comp)
    comp.set_editor_property("translucency_sort_priority", -20 + level)
p.niagara_actor(unreal.load_asset(p.ROOT + "NS_BerlinAtmo_MitteSmoke"), unreal.Vector(cx, cy - 1500, cz), "ATM_MitteSmoke_T", "Atm")
p.niagara_actor(unreal.load_asset(p.ROOT + "NS_BerlinAtmo_CraterVapour"), unreal.Vector(cx + 1200, cy + 2000, cz + 30), "ATM_Crater_T", "Atm")
p.niagara_actor(unreal.load_asset(p.ROOT + "NS_BerlinAtmo_Smoulder_Shell"), unreal.Vector(cx - 900, cy - 1200, cz + 60), "ATM_Smoulder_T", "Atm")
p.niagara_actor(unreal.load_asset(p.ROOT + "NS_BerlinAtmo_Fire_Ruin"), unreal.Vector(cx + 1500, cy - 1600, cz + 30), "ATM_Fire_T", "Atm")
light = s.SUB.spawn_actor_from_class(unreal.PointLight, unreal.Vector(cx + 1500, cy - 1600, cz + 290), unreal.Rotator(0, 0, 0))
light.set_actor_label("ATM_FireLight_T")
light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
p.setup_light(light.light_component, p.FIRE_LIGHT, unreal.load_asset(p.ROOT + "MI_LF_BerlinAtmo_Flame_0"))
glow = s.SUB.spawn_actor_from_class(unreal.PointLight, unreal.Vector(cx + 1200, cy + 2000, cz + 900), unreal.Rotator(0, 0, 0))
glow.set_actor_label("ATM_CraterLight_T")
glow.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
p.setup_light(glow.light_component, p.CRATER_LIGHT, unreal.load_asset(p.ROOT + "M_LF_BerlinAtmo_Crater"))

# heat shimmer cards: one over the riflemen, one at the edge of their sight (the fog of war edge)
for index, (dx, dy) in enumerate(((0.0, 0.0), (3600.0, 0.0), (0.0, 3800.0))):
    card = s.SUB.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(cx + dx, cy + dy, cz + 250.0), unreal.Rotator(0, 0, 0))
    card.set_actor_scale3d(unreal.Vector(26.0, 26.0, 1.0))
    card.set_actor_label("ATM_Shimmer_T%d" % index)
    card.static_mesh_component.set_static_mesh(plane)
    card.static_mesh_component.set_material(0, unreal.load_asset(p.ROOT + "M_BerlinAtmo_ShimmerCard"))
    p.quiet(card.static_mesh_component)
MPC = unreal.load_asset(p.ROOT + "MPC_BerlinAtmosphere")


def shimmer(value):
    unreal.MaterialLibrary.set_scalar_parameter_value(s.game_world(), MPC, "CraterShimmer", value)
    return "CraterShimmer %s" % value


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
S = {"phase": "start", "t": time.time(), "i": 0, "info": {}}


def pc():
    return unreal.GameplayStatics.get_player_controller(s.game_world(), 0)


def move(dx, dy):
    pawn = pc().get_controlled_pawn()
    location = pawn.get_actor_location()
    pawn.set_actor_location(unreal.Vector(cx + dx, cy + dy, location.z), False, True)
    return "pawn %s at %s, camera %s" % (pawn.get_class().get_name(), pawn.get_actor_location(),
                                         unreal.GameplayStatics.get_player_camera_manager(s.game_world(), 0).get_camera_location())


def select():
    units = [a for a in unreal.GameplayStatics.get_all_actors_of_class(s.game_world(), unreal.Pawn)
             if (a.get_actor_location() - unreal.Vector(cx, cy, cz)).length() < 2500.0 and a != pc().get_controlled_pawn() and "Inf" in a.get_class().get_name()]
    done = []
    for unit in units[:6]:
        try:
            pc().select_single_unit_from_ui(unit)
            done.append(unit.get_class().get_name())
            break
        except Exception as error:  # noqa: BLE001
            done.append("failed %s: %s" % (unit.get_class().get_name(), error))
    return "units near %d, selected %s" % (len(units), done)


ONLY = ["Shimmer"]      # second half: the shimmer alone, so nothing else explains a difference
STEPS = [
    (None, lambda: move(0, 0), 16.0),
    ("game_units_on", None, 1.0), ("game_units_on_b", None, 3.0),
    ("game_units_off", lambda: s.game_show(False), 2.0),
    ("game_selected_on", lambda: (s.game_show(True), select())[1], 3.0),
    ("game_selected_off", lambda: s.game_show(False), 2.0),
    ("game_shimmer_units_on", lambda: (s.game_show(True, ONLY), shimmer(1.0), select())[2], 3.0),
    ("game_shimmer_units_on_b", None, 0.6),
    ("game_shimmer_units_off", lambda: shimmer(0.0), 2.0),
    ("game_shimmer_units_x3", lambda: shimmer(3.0), 2.0),
    (None, lambda: (shimmer(1.0), move(3600, 0))[1], 5.0),
    ("game_shimmer_fogedge_on", None, 1.0), ("game_shimmer_fogedge_off", lambda: shimmer(0.0), 2.0),
    (None, lambda: (shimmer(1.0), move(0, 3800))[1], 5.0),
    ("game_shimmer_fogedge2_on", None, 1.0), ("game_shimmer_fogedge2_off", lambda: shimmer(0.0), 2.0),
    (None, None, 2.0),
]


def tick():
    now = time.time()
    if S["phase"] == "start":
        les.editor_request_begin_play()
        S["phase"], S["t"] = "wait", now
        return False
    if S["phase"] == "wait":
        if s.game_world() is None or now - S["t"] < 16.0:
            return False
        unreal.GameplayStatics.set_game_paused(s.game_world(), False)
        print("controller", pc().get_class().get_name(), "view target", pc().get_view_target().get_class().get_name(), "ours", [len(v) for v in s.game_ours()])
        S["phase"], S["t"], S["wait"] = "steps", now, 1.0
        return False
    if S["phase"] == "steps":
        if now - S["t"] < S["wait"]:
            return False
        if S.get("pending"):
            unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, OUT + S.pop("pending") + ".png")
            S["t"], S["wait"] = now, 1.5
            return False
        if S["i"] >= len(STEPS):
            les.editor_request_end_play()
            S["phase"], S["t"] = "ending", now
            return False
        name, before, wait = STEPS[S["i"]]
        S["i"] += 1
        if before:
            print(name, "->", before())
        if name:
            S["pending"] = name
        S["t"], S["wait"] = now, wait
        return False
    return (not les.is_in_play_in_editor()) and now - S["t"] > 3.0


def safe():
    try:
        return tick()
    except Exception:
        if les.is_in_play_in_editor():
            les.editor_request_end_play()
        raise


_defer = safe
