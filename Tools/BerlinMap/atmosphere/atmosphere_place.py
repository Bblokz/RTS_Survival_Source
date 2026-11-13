"""Places the Ger_Berlin atmosphere per district from Saved/Berlin/terrain/Ger_Berlin_atmosphere.json (atmosphere_gen.py).

In the map build (berlin_map_build.step_atmosphere calls this):

    import atmosphere_place; atmosphere_place.run()

What it makes; every actor label and component name starts with ATM_, every component carries the tag BerlinAtmosphere,
so the step can be cleared and redone on its own (clear()):
  ATM_Sheet_L<n>_NNN     StaticMeshActor, the engine's 1 m plane scaled to one 80 m tile, M_BerlinAtmo_Sheet: the low smoke
                         of Mitte, the green mist of the crater and the smoke of the east (which is which comes from the
                         district mask the material reads, not from the actor)
  ATM_Smoulder_<class>_NNN   NiagaraActor, NS_BerlinAtmo_Smoulder_Wisp / _Shell / _Hero: smouldering ruins and collapse heaps,
                         map-wide, at fixed places (not attached to the ruin)
  ATM_MitteSmoke_NN      NiagaraActor, NS_BerlinAtmo_MitteSmoke, one per ruin block of Mitte
  ATM_Crater             NiagaraActor, NS_BerlinAtmo_CraterVapour (green vapour) at the bowl's floor
  ATM_Shimmer_N          StaticMeshActor, the 1 m plane scaled to a 26 m card over the bowl, M_BerlinAtmo_ShimmerCard (refraction only)
  ATM_CraterLight        PointLight, dim green, unshadowed, light function M_LF_BerlinAtmo_Crater
  ATM_Fire_N             NiagaraActor (NS_BerlinAtmo_Fire_Ruin / _Wreck) and PointLight (light function MI_LF_BerlinAtmo_Fire_n),
  ATM_FireLight_N        standing on their host (a hero ruin or a wreck), not attached, tagged AtmHost_<host label>.
                         See place_fires() for what happens when the host dies.
Nothing here has collision, affects navigation, casts shadows or ticks an actor. No engine fog of any kind is used; the
step removes an ExponentialHeightFog if the level has one (the map's rule).

Kept free of 'os' and of everything outside 'unreal' and 'json': the shared editor's script sandbox imports this file.
"""
import json

import unreal

PLACEMENT_FILE = "D:/UE5Projects/RTS_Survival/Saved/Berlin/terrain/Ger_Berlin_atmosphere.json"
ROOT = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Atmosphere/"
PLANE = "/Engine/BasicShapes/Plane"
PREFIX = "ATM_"
TAG = "BerlinAtmosphere"
FOLDER = "Berlin/Atmosphere"

# lights (the knobs EastFire and CraterGlow scale them through their light functions; these are the values at knob 2)
FIRE_LIGHT = dict(intensity=36.0, radius=1150.0, colour=(1.0, 0.50, 0.17), source_radius=45.0)
CRATER_LIGHT = dict(intensity=0.7, radius=7500.0, colour=(0.42, 1.0, 0.30), height=950.0)
FIRE_SCALE = {"Ruin": 1.0, "Wreck": 1.0}        # component scale of the copied fire (the smoke column ignores it)
HOST_TOLERANCE = 150.0                          # cm between the host's position in the dress file and the actor found


def log(text):
    unreal.log_warning("BERLIN_ATMOSPHERE " + str(text))


def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def sds():
    return unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def handle_object(handle):
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))


def is_ours(component):
    if component is None or not isinstance(component, unreal.ActorComponent):
        return False
    return TAG in [str(t) for t in component.get_editor_property("component_tags")]


def clear():
    """Removes every ATM_ actor and every atmosphere component hung on a host."""
    actors = components = 0
    for actor in actors_sub().get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            actors_sub().destroy_actor(actor)
            actors += 1
            continue
        if not actor.get_components_by_tag(unreal.ActorComponent, TAG):
            continue
        handles = sds().k2_gather_subobject_data_for_instance(actor)
        doomed = [h for h in handles if is_ours(handle_object(h))]
        if doomed:
            components += sds().delete_subobjects(handles[0], doomed)
    if actors or components:
        unreal.SystemLibrary.collect_garbage()
    return actors, components


def remove_engine_fog():
    removed = 0
    for actor in actors_sub().get_all_level_actors():
        if actor.get_class().get_name() == "ExponentialHeightFog":
            actors_sub().destroy_actor(actor)
            removed += 1
    return removed


def quiet(component):
    """No collision, no navigation, no shadow, tagged."""
    if isinstance(component, unreal.PrimitiveComponent):
        # the PROFILE, not only the enabled flag: a static mesh component that still uses its mesh's default collision
        # came back from the saved map as BlockAll (the engine plane's own setting), and the sheets blocked traces
        component.set_collision_profile_name("NoCollision")
        try:
            component.set_editor_property("use_default_collision", False)
        except Exception:  # noqa: BLE001
            pass
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("generate_overlap_events", False)
        component.set_editor_property("cast_shadow", False)
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_editor_property("component_tags", [TAG])


# ---- sheets -----------------------------------------------------------------------------------------------------------------
def place_sheets(sheets):
    plane = unreal.load_asset(PLANE)
    material = unreal.load_asset(ROOT + "M_BerlinAtmo_Sheet")
    if plane is None or material is None:
        raise RuntimeError("M_BerlinAtmo_Sheet missing: run atmosphere_build.py")
    scale = sheets["tile"] / 100.0
    count = 0
    for index, (x, y, layers) in enumerate(sheets["tiles"]):
        for level in range(layers):
            z = sheets["base_z"] + sheets["heights"][level]
            actor = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, 0.0))
            actor.set_actor_scale3d(unreal.Vector(scale, scale, 1.0))
            actor.set_actor_label("%sSheet_L%d_%03d" % (PREFIX, level, index))
            actor.set_folder_path(FOLDER + "/Sheets")
            comp = actor.static_mesh_component
            comp.set_static_mesh(plane)
            comp.set_material(0, material)
            quiet(comp)
            comp.set_editor_property("translucency_sort_priority", -20 + level)     # under every other effect
            comp.set_editor_property("receives_decals", False)
            comp.set_editor_property("affect_distance_field_lighting", False)
            comp.set_editor_property("affect_dynamic_indirect_lighting", False)
            comp.set_editor_property("visible_in_ray_tracing", False)
            count += 1
    return count


# ---- sprite systems ------------------------------------------------------------------------------------------------------------
def niagara_actor(system, location, label, folder, scale=1.0):
    actor = actors_sub().spawn_actor_from_object(system, location, unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    if scale != 1.0:
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    comp = actor.get_component_by_class(unreal.NiagaraComponent)
    if comp is not None:
        quiet(comp)
    return actor


def place_mitte_smoke(items):
    system = unreal.load_asset(ROOT + "NS_BerlinAtmo_MitteSmoke")
    if system is None:
        raise RuntimeError("NS_BerlinAtmo_MitteSmoke missing: run atmosphere_build.py")
    for index, (x, y, z, _density) in enumerate(items):
        niagara_actor(system, unreal.Vector(x, y, z), "%sMitteSmoke_%02d" % (PREFIX, index), FOLDER + "/MitteSmoke")
    return len(items)


def place_smoulder(items):
    """One NiagaraActor per smouldering ruin or collapse heap, at a fixed place in the map (not attached: when the ruin
    dies the smoke keeps rising from its remnant or heap)."""
    systems = {key: unreal.load_asset(ROOT + "NS_BerlinAtmo_Smoulder_" + key) for key in ("Wisp", "Shell", "Hero")}
    if None in systems.values():
        raise RuntimeError("smoulder systems missing: run atmosphere_build.py")
    for index, (x, y, z, kind, _host) in enumerate(items):
        niagara_actor(systems[kind], unreal.Vector(x, y, z), "%sSmoulder_%s_%03d" % (PREFIX, kind, index), FOLDER + "/Smoulder")
    return len(items)


def setup_light(comp, settings, function):
    comp.set_editor_property("intensity_units", unreal.LightUnits.UNITLESS)
    comp.set_intensity(settings["intensity"])
    comp.set_attenuation_radius(settings["radius"])
    comp.set_light_color(unreal.LinearColor(settings["colour"][0], settings["colour"][1], settings["colour"][2], 1.0))
    comp.set_cast_shadows(False)
    comp.set_editor_property("source_radius", settings.get("source_radius", 0.0))
    comp.set_editor_property("use_inverse_squared_falloff", False)      # a plain falloff: the radius is what the owner tunes
    comp.set_editor_property("light_falloff_exponent", 2.4)
    comp.set_editor_property("affect_translucent_lighting", False)
    comp.set_editor_property("indirect_lighting_intensity", 0.0)
    comp.set_editor_property("volumetric_scattering_intensity", 0.0)
    comp.set_editor_property("specular_scale", 0.3)
    comp.set_light_function_material(function)
    comp.set_editor_property("disabled_brightness", 0.5)
    comp.set_editor_property("can_ever_affect_navigation", False)
    comp.set_editor_property("component_tags", [TAG])


def place_crater(crater):
    system = unreal.load_asset(ROOT + "NS_BerlinAtmo_CraterVapour")
    function = unreal.load_asset(ROOT + "M_LF_BerlinAtmo_Crater")
    if system is None or function is None:
        raise RuntimeError("crater assets missing: run atmosphere_build.py")
    niagara_actor(system, unreal.Vector(crater["x"], crater["y"], crater["floor_z"]), PREFIX + "Crater", FOLDER + "/Crater")
    light = actors_sub().spawn_actor_from_class(unreal.PointLight, unreal.Vector(crater["x"], crater["y"], crater["rim_z"] + CRATER_LIGHT["height"]),
                                                unreal.Rotator(0.0, 0.0, 0.0))
    light.set_actor_label(PREFIX + "CraterLight")
    light.set_folder_path(FOLDER + "/Crater")
    light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    setup_light(light.light_component, CRATER_LIGHT, function)
    plane = unreal.load_asset(PLANE)
    shimmer = unreal.load_asset(ROOT + "M_BerlinAtmo_ShimmerCard")
    if shimmer is None:
        raise RuntimeError("M_BerlinAtmo_ShimmerCard missing: run atmosphere_build.py")
    for index, (x, y, z, size) in enumerate(crater.get("shimmer_cards", [])):
        card = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, 0.0))
        card.set_actor_scale3d(unreal.Vector(size / 100.0, size / 100.0, 1.0))
        card.set_actor_label("%sShimmer_%d" % (PREFIX, index))
        card.set_folder_path(FOLDER + "/Crater")
        comp = card.static_mesh_component
        comp.set_static_mesh(plane)
        comp.set_material(0, shimmer)
        quiet(comp)
        comp.set_editor_property("receives_decals", False)
        comp.set_editor_property("visible_in_ray_tracing", False)
    return 1 + len(crater.get("shimmer_cards", []))


# ---- fires ------------------------------------------------------------------------------------------------------------------
def find_host(fire, by_label):
    actor = by_label.get(fire["host"])
    if actor is None:
        return None
    location = actor.get_actor_location()
    if abs(location.x - fire["host_x"]) > HOST_TOLERANCE or abs(location.y - fire["host_y"]) > HOST_TOLERANCE:
        log("host %s is not where the dress file has it (%.0f %.0f): free actors instead" % (fire["host"], location.x, location.y))
        return None
    return actor


def place_fires(fires, lights):
    """A NiagaraActor and a PointLight per fire, standing on its host (a hero ruin or a wreck) but NOT attached to it:
    a ruin that dies sinks and tilts, and an attached fire would sink and lean its smoke with it, while the engine would
    still not destroy it. Each carries the tags BerlinAtmosphere and AtmHost_<host label>, so a death graph that wants
    the fire gone can find it. (Components added to the host through the subobject subsystem were tried first: a
    Niagara component made that way never activated, in the editor or in the game.)"""
    systems = {key: unreal.load_asset(ROOT + "NS_BerlinAtmo_Fire_" + key) for key in ("Ruin", "Wreck")}
    functions = [unreal.load_asset(ROOT + "MI_LF_BerlinAtmo_Flame_%d" % i) for i in range(6)]
    if None in systems.values() or None in functions:
        raise RuntimeError("fire assets missing: run atmosphere_build.py")
    by_label = {a.get_actor_label(): a for a in actors_sub().get_all_level_actors()}
    light_of = {entry["fire"]: entry for entry in lights}
    attached = free = 0
    for fire in fires:
        entry = light_of[fire["label"]]
        effect = niagara_actor(systems[fire["system"]], unreal.Vector(fire["x"], fire["y"], fire["z"]), fire["label"],
                               FOLDER + "/Fires", FIRE_SCALE[fire["system"]])
        light = actors_sub().spawn_actor_from_class(unreal.PointLight, unreal.Vector(entry["x"], entry["y"], entry["z"]),
                                                    unreal.Rotator(0.0, 0.0, 0.0))
        light.set_actor_label(entry["label"])
        light.set_folder_path(FOLDER + "/Fires")
        light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        setup_light(light.light_component, FIRE_LIGHT, functions[entry["phase"] % len(functions)])
        host = find_host(fire, by_label)
        for actor in (effect, light):
            actor.set_editor_property("tags", [TAG, "AtmHost_" + fire["host"]])
        attached += 1 if host is not None else 0
        free += 0 if host is not None else 1
    return attached, free


def run(placement_file=PLACEMENT_FILE):
    """The whole step on the open level (berlin_map_build.step_atmosphere checks that it is Ger_Berlin first)."""
    with open(placement_file, "r") as handle:
        data = json.load(handle)
    removed = clear()
    fog = remove_engine_fog()
    sheets = place_sheets(data["sheets"])
    smoulder = place_smoulder(data.get("smoulder", []))
    smoke = place_mitte_smoke(data["mitte_smoke"])
    crater = place_crater(data["crater"])
    fires = place_fires(data["fires"], data["fire_lights"])
    result = dict(removed_actors=removed[0], removed_components=removed[1], engine_fog_removed=fog, sheets=sheets,
                  smoulder=smoulder, mitte_smoke=smoke, crater=crater, fires_on_host=fires[0], fires_host_missing=fires[1])
    log("atmosphere: %s" % result)
    return result
