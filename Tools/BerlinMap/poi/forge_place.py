"""Places the Forge survey station bonus on Ger_Berlin (BerlinForgeStation.md; owner's pick A + B + C, 2026-11-22).
Runs inside the throwaway editor of forge_editor_server.py (forge_cmd.py), after forge_assets_build.py.

    exec(open(r'.../forge_place.py').read()); report = place_all(); save_when_nav_ready()

What it does (labels FORGE_*, folder Berlin/ForgeStation; never the POI_ prefix, which berlin_map_build clears):
- replaces the one-mesh POI_ForgeSurveyStation with ABerlinForgeStation (shell mesh, dressing, sedation cycle);
- places the three counted targets (BP_ForgeStation_Rig / _CryoBlock / _CommandBunker) at their split offsets;
- an AEnemyStrongpoint "ForgeStation" with named posts (units come from DA_BerlinForces_<difficulty>) and two rim
  sampling circuits for the hazmat crews;
- the Forge megaphone at the crew hut, sampling stakes and cryo canisters on the crater rim;
- UMission_BerlinForgeStation in the MissionManager's Missions.
Frames: station local = Unreal local of the station actor (cm); Blender numbers are converted with y -> -y.
"""
import json
import math

import unreal

HOME = "/Game/RTS_Survival/Blueprints/Environment/Berlin/ForgeStation"
SPLIT = HOME + "/Split"
DATA = "/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets"  # Berlin data assets live next to the map (moved 2026-11-24)
LAYOUT_JSON = ("D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinForgeStation/v3_split/"
               "split_layout.json")
TERRAIN_JSON = "D:/UE5Projects/RTS_Survival/Saved/Berlin/terrain/Ger_Berlin_layout.json"
FOLDER = "Berlin/ForgeStation"
OLD_LABEL = "POI_ForgeSurveyStation"

NS = {
    "stack": "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/"
             "Refinery_Meshes/FX/NS_RadixiteSmoke1",
    "vapour": "/Game/RTS_Survival/Blueprints/Environment/Berlin/Atmosphere/NS_BerlinAtmo_CraterVapour",
    "burst": "/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1/Niagara/NS_SmokeBurst1",
    "puff": "/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1/Niagara/NS_SmokePuff1",
    "death": "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke_Large",
}
SND = {
    "drill": "/Game/RTS_Survival/Sounds/Construction/S_ConstructionDrill",
    "winch": "/Game/RTS_Survival/Sounds/Crane/Crane_Idle",
    "radio1": "/Game/RTS_Survival/Sounds/Cinematics/RadioEffects/TurnOnRadio_1",
    "radio3": "/Game/RTS_Survival/Sounds/Cinematics/RadioEffects/TurnOnRadio_3",
    "geiger": "/Game/RTS_Survival/Sounds/Cinematics/Logo/LogoGeiger",
    "pump": HOME + "/Sounds/S_ForgeStation_PumpLoop",
    "compressor": HOME + "/Sounds/S_ForgeStation_CompressorRise",
    "vent": HOME + "/Sounds/S_ForgeStation_CryoVent",
}
ATTENUATION = "/Game/RTS_Survival/Sounds/Settings/Attachments/Att_Attachments"
WORK_MONTAGE = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/AimPosMontages/Scavenge_infMontage"
MEGAPHONE = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_Megaphone_Broadcast"
TARGET_ROLES = [("Rig", "BP_ForgeStation_Rig", "SM_ForgeStation_Rig", 1.6),
                ("CryoBlock", "BP_ForgeStation_CryoBlock", "SM_ForgeStation_CryoBlock", 2.0),
                ("CommandBunker", "BP_ForgeStation_CommandBunker", "SM_ForgeStation_CommandBunker", 1.6)]

ELL = unreal.EditorLevelLibrary
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def b2u(bx, by, bz=0.0):
    """Blender metres (station generator) -> station-local Unreal cm."""
    return unreal.Vector(bx * 100.0, -by * 100.0, bz * 100.0)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("missing asset " + path)
    return asset


def actors_by_label():
    return {a.get_actor_label(): a for a in EAS.get_all_level_actors()}


def station_transform():
    old = actors_by_label().get(OLD_LABEL)
    if old is not None:
        return old.get_actor_location(), old.get_actor_rotation()
    station = actors_by_label().get("FORGE_Station")
    return station.get_actor_location(), station.get_actor_rotation()


def to_world(station_loc, station_rot, local):
    return unreal.Transform(station_loc, station_rot, unreal.Vector(1, 1, 1)).transform_location(local)


def to_local(station_loc, station_rot, world):
    return unreal.Transform(station_loc, station_rot, unreal.Vector(1, 1, 1)).inverse_transform_location(world)


def ground_z(location, ignore=()):
    start = unreal.Vector(location.x, location.y, location.z + 3000.0)
    end = unreal.Vector(location.x, location.y, location.z - 3000.0)
    hit = unreal.SystemLibrary.line_trace_single(ELL.get_editor_world(), start, end,
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, list(ignore),
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return location.z
    hit_result = hit.to_tuple() if hasattr(hit, "to_tuple") else hit
    return hit_result[4].z if isinstance(hit_result, tuple) else location.z


def spawn(cls_or_asset, label, location, rotation):
    if isinstance(cls_or_asset, unreal.Blueprint):
        actor = EAS.spawn_actor_from_object(cls_or_asset, location, rotation)
    elif isinstance(cls_or_asset, unreal.StaticMesh):
        actor = EAS.spawn_actor_from_object(cls_or_asset, location, rotation)
    else:
        actor = EAS.spawn_actor_from_class(cls_or_asset, location, rotation)
    actor.set_actor_label(label)
    actor.set_folder_path(FOLDER)
    return actor


def clear_forge_actors():
    removed = []
    for actor in EAS.get_all_level_actors():
        if actor.get_actor_label().startswith("FORGE_"):
            removed.append(actor.get_actor_label())
            EAS.destroy_actor(actor)
    return removed


# ----- dressing (station-local cm; BerlinForgeStation.md section 3, options A and C) -----
def light(location, yaw_to_centre=True, spot=True, intensity=40.0, color=(1.0, 0.86, 0.62), radius=2200.0,
          pulse=False, pulse_seconds=2.4, role="None", pitch=-62.0):
    entry = unreal.ForgeStationLight()
    entry.set_editor_property("relative_location", location)
    yaw = math.degrees(math.atan2(-location.y, -location.x)) if yaw_to_centre else 0.0
    entry.set_editor_property("relative_rotation", unreal.Rotator(0.0, pitch, yaw))
    entry.set_editor_property("spot_light", spot)
    entry.set_editor_property("intensity", intensity)
    entry.set_editor_property("color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    entry.set_editor_property("attenuation_radius", radius)
    entry.set_editor_property("pulse", pulse)
    entry.set_editor_property("pulse_seconds", pulse_seconds)
    entry.set_editor_property("linked_role", role_enum(role))
    return entry


def role_enum(role):
    names = {"None": "NONE", "Rig": "RIG", "CryoBlock": "CRYO_BLOCK", "CommandBunker": "COMMAND_BUNKER"}
    return getattr(unreal.ForgeStationPartRole, names[role])


def effect(system, location, scale=1.0, use="Ambient", role="None"):
    entry = unreal.ForgeStationEffect()
    entry.set_editor_property("system", load(NS[system]))
    entry.set_editor_property("relative_transform", unreal.Transform(location, unreal.Rotator(0, 0, 0),
                                                                     unreal.Vector(scale, scale, scale)))
    entry.set_editor_property("use", unreal.ForgeStationDressingUse.SEDATION_VENT if use == "Vent"
                              else unreal.ForgeStationDressingUse.AMBIENT)
    entry.set_editor_property("linked_role", role_enum(role))
    return entry


def sound(key, location, use="Ambient", role="None", volume=1.0, repeat=0.0):
    entry = unreal.ForgeStationSound()
    entry.set_editor_property("sound", load(SND[key]))
    entry.set_editor_property("relative_location", location)
    entry.set_editor_property("use", unreal.ForgeStationDressingUse.SEDATION_VENT if use == "Vent"
                              else unreal.ForgeStationDressingUse.AMBIENT)
    entry.set_editor_property("linked_role", role_enum(role))
    entry.set_editor_property("volume_multiplier", volume)
    entry.set_editor_property("repeat_seconds", repeat)
    entry.set_editor_property("attenuation", load(ATTENUATION))
    return entry


def sound_repeat(key):
    """Loops keep playing; one-shot sounds are replayed by the station."""
    asset = load(SND[key])
    looping = bool(asset.get_editor_property("looping")) if isinstance(asset, unreal.SoundWave) else False
    duration = asset.get_editor_property("duration") if hasattr(asset, "get_editor_property") else 5.0
    return 0.0 if looping else max(float(duration) + 1.5, 4.0)


def build_dressing(split):
    rig = split_offset(split, "SM_ForgeStation_Rig")
    lights = [
        light(b2u(-4.5, -11.5, 7.0)), light(b2u(8.0, 11.0, 6.5)), light(b2u(-2.0, 12.5, 6.5)),
        light(b2u(15.5, -10.5, 7.5), role="Rig"),
        # beacon on the shard mast by the console, and the hung core in the rig tower
        light(b2u(-2.6, 6.0, 3.8), yaw_to_centre=False, spot=False, intensity=30.0, color=(1.0, 0.7, 0.2),
              radius=900.0, pulse=True, pulse_seconds=2.4),
        light(unreal.Vector(rig.x, rig.y, 600.0), yaw_to_centre=False, spot=False, intensity=20.0,
              color=(1.0, 0.62, 0.15), radius=800.0, pulse=True, pulse_seconds=3.5, role="Rig"),
    ]
    effects = [
        effect("stack", b2u(-9.6, -6.0, 9.4), 1.0, role="CryoBlock"),
        effect("vapour", b2u(11.3, 4.8, 1.0), 0.45),
    ]
    for k in range(4):
        angle = math.radians(45.0 + 90.0 * k)
        lance = unreal.Vector(rig.x + 180.0 * math.cos(angle), rig.y + 180.0 * math.sin(angle), 120.0)
        effects.append(effect("burst", lance, 0.6, use="Vent", role="Rig"))
    effects.append(effect("puff", b2u(3.2, -5.9, 7.6), 0.7, use="Vent"))
    sounds = [
        sound("drill", unreal.Vector(rig.x, rig.y, 300.0), role="Rig", volume=0.55, repeat=sound_repeat("drill")),
        sound("winch", unreal.Vector(rig.x, rig.y, 900.0), role="Rig", volume=0.4, repeat=sound_repeat("winch")),
        sound("pump", b2u(-9.6, -6.0, 2.0), role="CryoBlock", volume=0.7),
        sound("radio1", b2u(-9.0, 5.0, 3.5), role="CommandBunker", volume=0.5, repeat=25.0),
        sound("radio3", b2u(-9.0, 5.0, 3.5), role="CommandBunker", volume=0.5, repeat=37.0),
        sound("geiger", b2u(17.0, 0.0, 0.5), volume=0.6, repeat=14.0),
        sound("compressor", unreal.Vector(rig.x, rig.y, 200.0), use="Vent", role="Rig", volume=0.8),
        sound("vent", unreal.Vector(rig.x, rig.y, 150.0), use="Vent", role="Rig", volume=0.9),
    ]
    parts = []
    for mesh_name, role, speed in (("SM_ForgeStation_DrillString", "Rig", 50.0),
                                   ("SM_ForgeStation_Dish", "CommandBunker", 36.0)):
        part = unreal.ForgeStationRotatingPart()
        part.set_editor_property("mesh", load(SPLIT + "/" + mesh_name))
        offset = split_offset(split, mesh_name)
        part.set_editor_property("relative_transform", unreal.Transform(offset, unreal.Rotator(0, 0, split_yaw(split, mesh_name)),
                                                                        unreal.Vector(1, 1, 1)))
        part.set_editor_property("degrees_per_second", speed)
        part.set_editor_property("linked_role", role_enum(role))
        parts.append(part)
    sedation = unreal.ForgeSedationCycle()
    sedation.set_editor_property("parameter_collection", load(HOME + "/MPC_ForgeSedation"))
    return lights, effects, sounds, parts, sedation


# ----- split layout -----
def read_split():
    with open(LAYOUT_JSON, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _split_entry(split, mesh_name):
    meshes = split.get("meshes", split)
    if isinstance(meshes, dict):
        return meshes[mesh_name]
    return next(m for m in meshes if m.get("name") == mesh_name)


def split_offset(split, mesh_name):
    entry = _split_entry(split, mesh_name)
    offset = entry.get("pivot_offset_ue_cm") or entry.get("offset_ue_cm") or entry.get("offset")
    return unreal.Vector(float(offset[0]), float(offset[1]), float(offset[2]) if len(offset) > 2 else 0.0)


def split_yaw(split, mesh_name):
    entry = _split_entry(split, mesh_name)
    return float(entry.get("yaw_deg", entry.get("yaw", 0.0)))


# ----- crater rim stakes and work circuits -----
def rim_points_local(station_loc, station_rot):
    """Four stakes on the crater rim, two each side of the station, 1.5 m inside the rim line."""
    with open(TERRAIN_JSON, "r", encoding="utf-8") as handle:
        terrain = json.load(handle)
    crater = next(r for r in terrain["regions"] if r["id"] == "crater")
    local_pts = []
    for x, y in crater["points"]:
        local_pts.append(to_local(station_loc, station_rot, unreal.Vector(x, y, station_loc.z)))
    centre_world = unreal.Vector(crater["centre"][0], crater["centre"][1], station_loc.z)
    centre = to_local(station_loc, station_rot, centre_world)
    stakes = []
    for wanted_y in (1900.0, 2700.0, -1900.0, -2700.0):
        near = [p for p in local_pts if p.x < centre.x]
        best = min(near, key=lambda p: abs(p.y - wanted_y))
        inward = (centre - best)
        inward.z = 0.0
        inward = inward.normal() * 150.0
        stakes.append(unreal.Vector(best.x + inward.x, best.y + inward.y, 0.0))
    return stakes


def make_post(post_id, location, yaw, cover=True, circuit=None, work_seconds=25.0):
    post = unreal.EnemyStrongpointPost()
    post.set_editor_property("post_id", post_id)
    post.set_editor_property("post", unreal.Transform(location, unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1, 1, 1)))
    post.set_editor_property("dig_in", False)
    post.set_editor_property("take_nearby_cover", cover)
    if circuit:
        work = unreal.EnemyStrongpointWorkCircuit()
        work.set_editor_property("enabled", True)
        work.set_editor_property("points", circuit)
        work.set_editor_property("work_seconds", work_seconds)
        work.set_editor_property("work_montage", load(WORK_MONTAGE))
        post.set_editor_property("work_circuit", work)
    return post


def build_posts(stakes, split):
    rig = split_offset(split, "SM_ForgeStation_Rig")
    sample_door = b2u(11.4, 15.6)
    rig_foot = unreal.Vector(rig.x - 150.0, rig.y + 550.0, 0.0)
    circuit_a = [stakes[0], stakes[1], rig_foot]
    circuit_b = [stakes[2], stakes[3], sample_door]
    return [
        make_post("Arch", b2u(3.2, -11.0), 90.0),
        make_post("Console", b2u(-3.0, 9.0), 180.0),
        make_post("RigFoot", unreal.Vector(rig.x - 200.0, rig.y + 750.0, 0.0), 135.0),
        make_post("RimCrewA", unreal.Vector(stakes[0].x - 250.0, stakes[0].y, 0.0), 0.0, cover=False,
                  circuit=circuit_a),
        make_post("RimCrewB", unreal.Vector(stakes[2].x - 250.0, stakes[2].y, 0.0), 0.0, cover=False,
                  circuit=circuit_b),
        make_post("CrewHut", b2u(2.5, 12.0), -90.0, cover=False),
        make_post("CrewEscort", unreal.Vector(stakes[1].x - 400.0, stakes[1].y + 200.0, 0.0), 0.0),
    ]


# ----- the mission -----
def add_mission():
    manager = next(a for a in EAS.get_all_level_actors() if "MissionManager" in a.get_class().get_name())
    missions = list(manager.get_editor_property("missions"))
    missions = [m for m in missions if m is None or m.get_class().get_name() != "Mission_BerlinForgeStation"]
    mission = unreal.new_object(unreal.Mission_BerlinForgeStation, outer=manager)
    forces = {}
    # python's name for ERTSGameDifficulty collides with the FRTSGameDifficulty struct; take the enum type from the
    # struct's DifficultyLevel field instead.
    difficulty = type(unreal.RTSGameDifficulty().get_editor_property("difficulty_level"))
    for name, key in (("NewToRTS", "NEW_TO_RTS"), ("Normal", "NORMAL"), ("Hard", "HARD"), ("Brutal", "BRUTAL")):
        forces[getattr(difficulty, key)] = load(DATA + "/DA_BerlinForces_" + name)
    mission.set_editor_property("forces_per_difficulty", forces)
    missions.append(mission)
    manager.set_editor_property("missions", missions)
    return manager.get_actor_label(), len(missions)


def place_all():
    split = read_split()
    station_loc, station_rot = station_transform()
    removed = clear_forge_actors()
    report = {"removed": removed, "station": [station_loc.x, station_loc.y, station_loc.z, station_rot.yaw]}

    station = spawn(unreal.BerlinForgeStation, "FORGE_Station", station_loc, station_rot)
    station.get_editor_property("shell_mesh").set_editor_property("static_mesh", load(SPLIT + "/SM_ForgeStation_Shell"))

    targets = []
    for role, bp_name, mesh_name, death_scale in TARGET_ROLES:
        offset = split_offset(split, mesh_name)
        location = to_world(station_loc, station_rot, offset)
        rotation = unreal.Rotator(0.0, 0.0, station_rot.yaw + split_yaw(split, mesh_name))
        actor = spawn(load(HOME + "/" + bp_name), "FORGE_Target_" + role, location, rotation)
        entry = unreal.ForgeStationTarget()
        entry.set_editor_property("role", role_enum(role))
        entry.set_editor_property("target", actor)
        entry.set_editor_property("destroyed_mesh", load(SPLIT + "/" + mesh_name + "_Destroyed"))
        entry.set_editor_property("death_effect", load(NS["death"]))
        entry.set_editor_property("death_effect_scale", death_scale)
        targets.append(entry)
        report["target_" + role] = [location.x, location.y, location.z]
    station.set_editor_property("targets", targets)

    lights, effects, sounds, parts, sedation = build_dressing(split)
    station.set_editor_property("lights", lights)
    station.set_editor_property("effects", effects)
    station.set_editor_property("sounds", sounds)
    station.set_editor_property("rotating_parts", parts)
    station.set_editor_property("sedation", sedation)

    stakes = rim_points_local(station_loc, station_rot)
    report["stakes_local"] = [[round(s.x), round(s.y)] for s in stakes]
    stake_mesh = load(SPLIT + "/SM_ForgeStation_SampleStake")
    canister_mesh = load(SPLIT + "/SM_ForgeStation_CryoCanister")
    for index, stake in enumerate(stakes):
        world = to_world(station_loc, station_rot, stake)
        world.z = ground_z(world)
        spawn(stake_mesh, "FORGE_Stake_%d" % (index + 1), world, unreal.Rotator(0, 0, 37.0 * index))
        canister = to_world(station_loc, station_rot, unreal.Vector(stake.x - 90.0, stake.y + 60.0, 0.0))
        canister.z = ground_z(canister)
        spawn(canister_mesh, "FORGE_Canister_%d" % (index + 1), canister, unreal.Rotator(0, 0, 21.0 * index))
    for index, local in enumerate((b2u(10.6, 15.4), b2u(12.2, 15.6))):
        world = to_world(station_loc, station_rot, local)
        world.z = ground_z(world)
        spawn(canister_mesh, "FORGE_Canister_Bunker_%d" % (index + 1), world, unreal.Rotator(0, 0, 50.0 * index))

    strongpoint = spawn(unreal.EnemyStrongpoint, "FORGE_Strongpoint", station_loc, station_rot)
    strongpoint.set_editor_property("strongpoint_id", "ForgeStation")
    strongpoint.set_editor_property("posts", build_posts(stakes, split))
    strongpoint.set_editor_property("units_set_by_mission", True)
    timing = strongpoint.get_editor_property("timing")
    timing.set_editor_property("spawn_on_begin_play", False)
    strongpoint.set_editor_property("timing", timing)

    megaphone_loc = to_world(station_loc, station_rot, b2u(-4.5, 13.5))
    megaphone_loc.z = ground_z(megaphone_loc)
    spawn(load(MEGAPHONE), "FORGE_Megaphone", megaphone_loc, unreal.Rotator(0, 0, station_rot.yaw + 180.0))

    report["mission"] = add_mission()
    old = actors_by_label().get(OLD_LABEL)
    if old is not None:
        EAS.destroy_actor(old)
        report["removed_old_poi"] = True
    print(json.dumps(report, indent=1, default=str))
    return report


SAVE_STATE = {"ticks": 0}


def save_when_nav_ready():
    """Waits for the navmesh rebuild (headless-map-edit-full-editor memory), then saves the map."""
    SAVE_STATE["ticks"] = 0

    def _defer():
        SAVE_STATE["ticks"] += 1
        world = ELL.get_editor_world()
        if SAVE_STATE["ticks"] < 60:
            return False
        if unreal.NavigationSystemV1.is_navigation_being_built(world):
            return False
        ok = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        print("saved", ok, "after ticks", SAVE_STATE["ticks"])
        return True

    globals()["_defer"] = _defer
