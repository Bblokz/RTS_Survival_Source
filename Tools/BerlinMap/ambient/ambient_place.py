"""Places the Ger_Berlin ambient motion from Saved/Berlin/terrain/Ger_Berlin_ambient.json (ambient_gen.py).

In the map build (berlin_map_build.step_ambient calls this):

    import ambient_place; ambient_place.run()

What it makes (everything it makes starts with AMB_, so it can be cleared and redone on its own):
  AMB_Litter_NNN        NiagaraActor per lane stretch, all on the ONE system NS_BerlinStreetLitter (folder Berlin/Ambient/Litter)
  AMB_<Mesh>_NN         StaticMeshComponent ON THE RUIN ACTOR it hangs from (attached to its root): curtains, shutters,
                        cables, lamps, wire spans. No collision, no shadow, no tick, no navigation; world position offset
                        stops beyond WPO_DISABLE_DISTANCE and the component is not drawn beyond DRAW_DISTANCE.
  AMB_Crows             one InstancedStaticMeshComponent per crow group, on its ruin; per-instance custom data
                        0 phase seconds, 1 rate, 2 airborne (see BerlinAmbientMotion.md: the crow manager contract)
  AMB_CrowFlock_N       the project's BP_BirdFlock_Crow (ABirdFlock), circling over the ruins (folder Berlin/Ambient/Crows)
Because the dressing and the perched crows are components of the ruin, they move, sink and disappear with it.

Nothing here touches navigation (no collision anywhere, can_ever_affect_navigation off): no navmesh rebuild is needed.
The step needs the RUIN_ actors of berlin_map_build.step_ruins() in the level; rerun it after step_ruins().
"""
import json

import unreal

import ambient_gen

AMBIENT_FILE = ambient_gen.OUT_FILE
DRESS_FILE = ambient_gen.DRESS_FILE
ROOT = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Ambient/"
LITTER_SYSTEM = ROOT + "Litter/NS_BerlinStreetLitter"
DRESSING = ROOT + "Dressing/"
CROW_MESH = DRESSING + "SM_BerlinAmb_CrowPerched"
CROW_FLOCK_BP = "/Game/RTS_Survival/Blueprints/Environment/Animated/Birds/DesertFlocks/BP_BirdFlock_Crow"
PREFIX = "AMB_"
TAG = "BerlinAmbient"               # component tag on everything placed here: one handle for a settings or death hook
WPO_DISABLE_DISTANCE = 9000         # cm; the RTS camera's far view corner is about 75 m away at full zoom-out
DRAW_DISTANCE = 13000.0             # cm; dressing further than this from the camera is not drawn at all
CROW_DRAW_DISTANCE = 11000.0
CROW_INSTANCE_FLOATS = 3            # 0 phase seconds, 1 rate, 2 airborne


def log(text):
    unreal.log_warning("BERLIN_AMBIENT " + str(text))


def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def sds():
    return unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def handle_object(handle):
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))


def ruin_lookup():
    """RUIN_<piece>_<index> actors of step_ruins(), by index."""
    found = {}
    for actor in actors_sub().get_all_level_actors():
        label = actor.get_actor_label()
        if label.startswith("RUIN_"):
            try:
                found[int(label.rsplit("_", 1)[1])] = actor
            except ValueError:
                pass
    return found


def check_stamp(data, lookup):
    """The relative transforms only fit the ruins they were generated for."""
    with open(DRESS_FILE, "r") as handle:
        ruins = json.load(handle)["ruins"]
    stamp = ambient_gen.ruin_stamp(ruins)
    if stamp != data["stamp"]:
        raise RuntimeError("Ger_Berlin_ambient.json was made for other ruins (%s, dress file has %s): run ambient_gen.py" % (data["stamp"], stamp))
    wrong = [i for i, actor in lookup.items() if i >= len(ruins) or ("_" + ruins[i]["piece"] + "_") not in actor.get_actor_label()]
    if wrong:
        raise RuntimeError("%d RUIN_ actors do not match the dress file (first index %d): rerun step_ruins()" % (len(wrong), wrong[0]))
    return ruins


def is_ambient(component):
    """Ours by tag; the name is only for the outliner (a rename can fail when an old component still holds the name)."""
    if component is None or not isinstance(component, unreal.ActorComponent):
        return False
    return TAG in [str(t) for t in component.get_editor_property("component_tags")] or component.get_name().startswith(PREFIX)


def clear(lookup=None):
    """Removes every AMB_ actor and every ambient component on the ruins."""
    actors = 0
    for actor in actors_sub().get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            actors_sub().destroy_actor(actor)
            actors += 1
    components = 0
    for actor in (lookup if lookup is not None else ruin_lookup()).values():
        handles = sds().k2_gather_subobject_data_for_instance(actor)
        doomed = [h for h in handles if is_ambient(handle_object(h))]
        if doomed:
            components += sds().delete_subobjects(handles[0], doomed)
    if actors or components:
        unreal.SystemLibrary.collect_garbage()      # frees the component names for the placement that follows
    return actors, components


def mesh_handle(actor):
    """The handle new components are added under: the actor itself, which attaches them to its root. (The handles of a
    Blueprint's own components point at the class templates, not at this actor's components, so the ruin's mesh cannot be
    picked as the parent here; the root does the same job: the dressing moves and dies with the actor.)"""
    return sds().k2_gather_subobject_data_for_instance(actor)[0]


def add_component(actor, parent, cls, name):
    handle, _fail = sds().add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls))
    comp = handle_object(handle)
    for suffix in ("", "_b", "_c", "_d"):
        sds().rename_subobject(handle, unreal.Text(name + suffix))
        if comp.get_name().startswith(PREFIX):
            break
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("can_ever_affect_navigation", False)
    comp.set_editor_property("cast_shadow", False)          # WPO shadows would be redrawn every frame (virtual shadow maps)
    comp.set_editor_property("generate_overlap_events", False)
    comp.set_editor_property("component_tags", [TAG])
    return comp


# ---- the four parts ------------------------------------------------------------------------------------------------------
def place_litter(items, folder="Berlin/Ambient/Litter"):
    system = unreal.load_asset(LITTER_SYSTEM)
    if system is None:
        log("missing " + LITTER_SYSTEM + ": run ambient_litter_build.py")
        return 0
    for index, (x, y, z, yaw, width) in enumerate(items):
        actor = actors_sub().spawn_actor_from_object(system, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_scale3d(unreal.Vector(1.0, width, 1.0))
        actor.set_actor_label("%sLitter_%03d" % (PREFIX, index))
        actor.set_folder_path(folder)
        comp = actor.get_component_by_class(unreal.NiagaraComponent)
        if comp is not None:
            comp.set_editor_property("component_tags", [TAG])
            comp.set_editor_property("cast_shadow", False)
    return len(items)


def place_dressing(items, lookup):
    meshes, parents, counts, placed, skipped = {}, {}, {}, 0, 0
    for index, mesh_name, x, y, z, yaw, pitch, roll, sx, sy, sz in items:
        actor = lookup.get(index)
        if actor is None:
            skipped += 1
            continue
        if mesh_name not in meshes:
            meshes[mesh_name] = unreal.load_asset(DRESSING + mesh_name)
        if meshes[mesh_name] is None:
            skipped += 1
            continue
        if index not in parents:
            parents[index] = mesh_handle(actor)
        counts[index] = counts.get(index, 0) + 1
        comp = add_component(actor, parents[index], unreal.StaticMeshComponent,
                             "%s%s_%02d" % (PREFIX, mesh_name.replace("SM_BerlinAmb_", ""), counts[index]))
        comp.set_static_mesh(meshes[mesh_name])
        comp.set_relative_transform(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(roll, pitch, yaw), unreal.Vector(sx, sy, sz)),
                                    False, False)
        comp.set_editor_property("world_position_offset_disable_distance", WPO_DISABLE_DISTANCE)
        comp.set_editor_property("ld_max_draw_distance", DRAW_DISTANCE)
        placed += 1
    return placed, skipped


def place_crows(groups, lookup):
    mesh = unreal.load_asset(CROW_MESH)
    birds = 0
    for group in groups:
        actor = lookup.get(group["ruin"])
        if actor is None or mesh is None:
            continue
        comp = add_component(actor, mesh_handle(actor), unreal.InstancedStaticMeshComponent, PREFIX + "Crows")
        comp.set_static_mesh(mesh)
        comp.set_editor_property("num_custom_data_floats", CROW_INSTANCE_FLOATS)
        comp.set_editor_property("ld_max_draw_distance", CROW_DRAW_DISTANCE)
        comp.set_editor_property("component_tags", [TAG, "BerlinCrowPerch"])
        for x, y, z, yaw, scale, phase, rate in group["birds"]:
            instance = comp.add_instance(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(scale, scale, scale)))
            comp.set_custom_data_value(instance, 0, phase, False)
            comp.set_custom_data_value(instance, 1, rate, False)
            comp.set_custom_data_value(instance, 2, 0.0, True)
            birds += 1
    return birds


def place_flocks(flocks, folder="Berlin/Ambient/Crows"):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(CROW_FLOCK_BP)
    if cls is None:
        log("missing " + CROW_FLOCK_BP)
        return 0
    for index, flock in enumerate(flocks):
        actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(flock["x"], flock["y"], flock["z"]), unreal.Rotator(0.0, 0.0, flock["yaw"]))
        ellipse = actor.get_editor_property("ellipse")
        ellipse.set_editor_property("semi_axis_x", flock["semi_x"])
        ellipse.set_editor_property("semi_axis_y", flock["semi_y"])
        ellipse.set_editor_property("clockwise", flock["clockwise"])
        actor.set_editor_property("ellipse", ellipse)
        flight = actor.get_editor_property("flight_settings")
        flight.set_editor_property("bird_count", flock["count"])
        flight.set_editor_property("flight_height", flock["height"])
        flight.set_editor_property("height_spread", 350.0)
        flight.set_editor_property("speed", 820.0)
        flight.set_editor_property("formation_length", 0.5)       # radians of this much smaller ellipse: a loose string of birds
        flight.set_editor_property("formation_width", 900.0)
        flight.set_editor_property("min_scale", 0.42)             # wingspan about 1 m: the preset's 0.6-0.72 suits open desert
        flight.set_editor_property("max_scale", 0.5)
        flight.set_editor_property("seed", flock["seed"])
        actor.set_editor_property("flight_settings", flight)
        actor.set_actor_label("%sCrowFlock_%d" % (PREFIX, index))
        actor.set_folder_path(folder)
    return len(flocks)


def run(ambient_file=AMBIENT_FILE):
    """The whole step on the open level (berlin_map_build.step_ambient checks that it is Ger_Berlin first)."""
    with open(ambient_file, "r") as handle:
        data = json.load(handle)
    lookup = ruin_lookup()
    check_stamp(data, lookup)
    removed = clear(lookup)
    litter = place_litter(data["litter"])
    dressing = place_dressing(data["dressing"], lookup)
    crows = place_crows(data["crow_groups"], lookup)
    flocks = place_flocks(data["crow_flocks"])
    result = dict(removed_actors=removed[0], removed_components=removed[1], litter=litter, dressing=dressing[0],
                  dressing_skipped=dressing[1], perched_crows=crows, crow_groups=len(data["crow_groups"]), flocks=flocks)
    log("ambient: %s" % result)
    return result
