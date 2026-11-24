"""Generic shot builder of the intro cinematic: turns the shot dicts of intro_manifest.py into subsequences
(camera, spawnables with keys, Material Parameter Collection keys) and the master sequence, and writes the cue table.

Times inside a shot are FRACTIONS of the shot (0..1) unless a field says seconds, because a shot's length follows its
voice lines. Run inside the throwaway editor with the cinematic map open.
"""
import importlib
import json
import math

import unreal

import intro_build as b
import intro_lib as lib
import intro_materials as mats
import intro_scout as scout

importlib.reload(b)

FRAME = unreal.FrameNumber
TICKS_PER_SECOND = 24000
PRE_ROLL_FRAMES = 6
SENSOR_WIDTH = 24.89   # the cine camera's default filmback (16:9 digital film), mm
actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_registry = unreal.AssetRegistryHelpers.get_asset_registry()
_asset_cache = {}


def find_asset(name):
    """Package path of the first asset with this exact name anywhere under /Game (cached)."""
    if name not in _asset_cache:
        found = [str(a.package_name) for a in _registry.get_assets_by_path("/Game", True) if str(a.asset_name) == name]
        _asset_cache[name] = found[0] if found else None
    return _asset_cache[name]


def resolve(path_or_name):
    if path_or_name.startswith("/"):
        return path_or_name
    return find_asset(path_or_name)


def focal_for_fov(fov_degrees):
    return SENSOR_WIDTH / 2.0 / math.tan(math.radians(fov_degrees) / 2.0)


def look_at(position, target):
    """(pitch, yaw, roll) that points a camera at 'position' toward 'target'."""
    dx, dy, dz = target[0] - position[0], target[1] - position[1], target[2] - position[2]
    return (math.degrees(math.atan2(dz, math.sqrt(dx * dx + dy * dy))), math.degrees(math.atan2(dy, dx)), 0.0)


def _ground(location, snap, snap_at=None):
    """snap: cm above the ground under the point (or under snap_at, for parts that must share one height)."""
    if snap is None:
        return location
    z = scout.ground_z(*(snap_at or (location[0], location[1])))
    return (location[0], location[1], (z if z is not None else 0.0) + snap + location[2])


def _key_transform(binding, keys, seconds, snap=None, base_scale=None, snap_at=None, local_offset=None):
    """keys: (fraction, location, rotation (pitch, yaw, roll), scale float or tuple)."""
    for old in binding.get_tracks():
        if isinstance(old, unreal.MovieScene3DTransformTrack):
            binding.remove_track(old)
    track = binding.add_track(unreal.MovieScene3DTransformTrack)
    section = track.add_section()
    section.set_range(0, b.frames(seconds))
    channels = section.get_all_channels()
    for key in keys:
        fraction, location, rotation = key[0], _ground(key[1], snap, snap_at), key[2]
        if isinstance(rotation, dict):      # {'at': point}: aim at a point (torches, spotlights)
            rotation = look_at(location, rotation["at"])
        if local_offset:                    # a part of a larger thing: offset in the thing's own frame (pitch and
            # roll included, or a tilted tank's hull and turret shear off its tracks)
            frame = unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1])
            axes = (frame.get_forward_vector(), frame.get_right_vector(), frame.get_up_vector())
            location = tuple(location[i] + sum(axis.to_tuple()[i] * offset for axis, offset in zip(axes, local_offset))
                             for i in range(3))
        scale = key[3] if len(key) > 3 else 1.0
        if not isinstance(scale, (tuple, list)):
            scale = (scale, scale, scale)
        if base_scale:
            scale = (scale[0] * base_scale[0], scale[1] * base_scale[1], scale[2] * base_scale[2])
        frame = FRAME(b.frames(fraction * seconds))
        values = [location[0], location[1], location[2], rotation[2], rotation[0], rotation[1], scale[0], scale[1], scale[2]]
        for channel, value in zip(channels[:9], values):
            channel.add_key(frame, value, interpolation=unreal.MovieSceneKeyInterpolation.LINEAR
                            if len(keys) > 2 else unreal.MovieSceneKeyInterpolation.AUTO)


def _key_spawn(binding, spawn, seconds):
    """spawn = (fraction on, fraction off): the actor only exists in between."""
    for track in binding.get_tracks():
        if isinstance(track, unreal.MovieSceneSpawnTrack):
            channel = track.get_sections()[0].get_all_channels()[0]
            channel.set_default(False)
            for key in channel.get_keys():
                channel.remove_key(key)
            if spawn[0] > 0.0:
                channel.add_key(FRAME(0), False)
            channel.add_key(FRAME(b.frames(spawn[0] * seconds)), True)
            if spawn[1] < 1.0:
                channel.add_key(FRAME(b.frames(spawn[1] * seconds)), False)


def _make_instance(spec):
    """Temporary editor actor configured from the spec; it becomes the spawnable's template."""
    kind = spec["kind"]
    where = unreal.Vector(0, 0, -300000)
    asset_path = resolve(spec["asset"]) if spec.get("asset") else None
    if spec.get("asset") and asset_path is None:
        # Modelled props that are not imported yet stand in as a greybox box or cylinder.
        if spec.get("fallback_box"):
            box = spec["fallback_box"]
            spec["_scale"] = (box[0] / 100.0, box[1] / 100.0, box[2] / 100.0)
            asset_path = "/Engine/BasicShapes/Cube"
        elif spec.get("fallback"):
            spec["_scale"] = (0.35, 0.35, 1.1)
            asset_path = "/Engine/BasicShapes/Cylinder"
            spec["mats"] = lib.ROOT + "/Materials/MI_Intro_Steel"
        else:
            print("  asset missing, skipped:", spec["asset"])
            return None
    asset = unreal.load_asset(asset_path) if asset_path else None
    if kind == "mesh":
        actor = actors_api.spawn_actor_from_class(unreal.StaticMeshActor, where)
        component = actor.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(asset)
        materials = spec.get("mats")
        if materials:
            if isinstance(materials, str):
                materials = [materials] * max(1, component.get_num_materials())
            for index, path in enumerate(materials):
                if path:
                    component.set_material(index, unreal.load_asset(resolve(path)))
        component.set_cast_shadow(spec.get("shadow", True))
    elif kind == "skel":
        actor = actors_api.spawn_actor_from_class(unreal.SkeletalMeshActor, where)
        actor.skeletal_mesh_component.set_skinned_asset_and_update(asset)
        if spec.get("mats"):
            for index in range(actor.skeletal_mesh_component.get_num_materials()):
                actor.skeletal_mesh_component.set_material(index, unreal.load_asset(resolve(spec["mats"])))
        for index, path in (spec.get("slot_mats") or {}).items():
            actor.skeletal_mesh_component.set_material(index, unreal.load_asset(path))
    elif kind == "class":
        actor = actors_api.spawn_actor_from_class(unreal.EditorAssetLibrary.load_blueprint_class(asset_path), where)
    elif kind == "niagara":
        actor = actors_api.spawn_actor_from_object(asset, where)
    elif kind == "decal":
        actor = actors_api.spawn_actor_from_class(unreal.DecalActor, where)
        actor.decal.set_decal_material(asset)
        size = spec.get("size", (200.0, 200.0, 200.0))
        actor.decal.set_editor_property("decal_size", unreal.Vector(*size))
    elif kind in ("point", "spot"):
        actor = actors_api.spawn_actor_from_class(unreal.PointLight if kind == "point" else unreal.SpotLight, where)
        component = actor.get_component_by_class(unreal.LightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_intensity_units(unreal.LightUnits.CANDELAS)
        component.set_intensity(spec.get("intensity", 100.0))
        component.set_light_color(unreal.LinearColor(*spec.get("color", (1.0, 1.0, 1.0, 1.0))))
        component.set_attenuation_radius(spec.get("radius", 2000.0))
        component.set_cast_shadows(spec.get("shadows", False))
        if kind == "spot":
            component.set_outer_cone_angle(spec.get("cone", 25.0))
            component.set_inner_cone_angle(spec.get("cone", 25.0) * 0.55)
    else:
        raise RuntimeError("unknown actor kind " + kind)
    return actor


# Unit Blueprints cannot be puppeteered by a transform track (their own movement code keeps them where they
# spawned), so a tank in the film is its meshes, taken from the unit Blueprint's own layout (offsets in cm from the
# unit origin, measured on a spawned instance): (kind, asset, offset, materials).
_T34 = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Meshes/"
_T34_TURRET = "/Game/RTS_Survival/Blueprints/Turrets/Russian/Medium/T34Turrets/"
_PANTHER = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/PantherG/Meshes/"
_IS2 = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Heavy/"
_IS2_TURRET = "/Game/RTS_Survival/Blueprints/Turrets/Russian/Heavy/IS-2/Meshes/"
TANK_KITS = {
    "Rus_Vehicle_T3476_Soviet": [
        ("skel", _T34 + "SK_T34_Tracks", (0.0, 0.0, 0.0), None),
        ("mesh", _T34 + "SM_T34_Hull", (0.0, -2.0, 3.0), _T34 + "MI_T34Soviet"),
        ("mesh", _T34_TURRET + "Meshes/SM_T34_76_TurretBaseMesh", (46.0, -1.0, 157.0), _T34 + "MI_T34Soviet"),
        ("skel", _T34_TURRET + "T34_76/Meshes/SK_34_76WeaponSystem", (46.0, -1.0, 157.0), _T34 + "MI_T34Soviet"),
    ],
    # IS-2 (Rus_Vehicle_IS-2_Soviet): hull, add-on panels and turret at scale 1.1 in the Blueprint (5th element);
    # the turret sits on the hull's Turret socket (64.7, -11.1, 162.5) x 1.1 + the hull offset.
    "Rus_Vehicle_IS-2_Soviet": [
        ("skel", _IS2 + "KV1/Meshes/SK_KV_1_Tracks", (0.0, 0.0, 0.0), None),
        ("mesh", _IS2 + "IS-1And-2/Meshes/SM_IS_1_Hull", (-8.9, 0.3, -4.7), _IS2 + "IS-1And-2/Materials/MI_IS_1_SovietCamo", 1.1),
        ("mesh", _IS2 + "IS-1And-2/IS2_Forge/SM_IS2_Soviet_Armor_R_05_AddOnPanels", (-8.9, 0.3, -4.7), None, 1.1),
        ("mesh", _IS2_TURRET + "SM_IS-2_BaseMesh", (62.3, -11.9, 174.0), None, 1.1),
        ("skel", _IS2_TURRET + "SK_IS-2_Turret", (62.3, -11.9, 174.0), None, 1.1),
    ],
    "Ger_Vehicle_PantherG": [
        ("skel", _PANTHER + "PantherG_Tracks", (0.0, 0.0, 0.0), None),
        ("mesh", _PANTHER + "SM_Hull_PantherG", (20.0, 10.0, 130.0), None),
        ("mesh", "/Game/RTS_Survival/Blueprints/Turrets/German/Heavy/PantherTurrets/PantherG/Meshes/SM_PantherG_Turret",
         (-30.0, 0.0, 180.0), None),
    ],
}


# Where a tank's tracks touch the ground (cm from the unit origin, forward / right) and how far the tracks sink in.
TANK_FOOTPRINT = {"Rus_Vehicle_T3476_Soviet": (205.0, 120.0), "Ger_Vehicle_PantherG": (200.0, 135.0)}
TANK_SINK = 4.0
# Track drive: the forward animation (wheels and sprockets turn) and the track material's pan rate, as the game sets
# them (PlayRate = speed / the unit's animation speed divider).
TANK_DRIVE = {
    "Rus_Vehicle_T3476_Soviet": ("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Anims/Anim_T34_Tracks_FWD",
                                 "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/"
                                 "DerivedUsedMutlipleTimes/MI_T34_Tracks", (1, 2)),
}


def _ground_fit(location, rotation, footprint, extra_z, z_from=50000.0):
    """Seats a tank on uneven ground: height, pitch and roll from the ground under its four track corners. A corner
    trace that lands on a prop (far off the others) is replaced by the median, so a rock does not tip the hull."""
    yaw = math.radians(rotation[1])
    fx, fy = math.cos(yaw), math.sin(yaw)
    rx, ry = -fy, fx
    length, width = footprint
    corners = {}
    for name, (a, c) in {"fl": (length, -width), "fr": (length, width), "bl": (-length, -width), "br": (-length, width)}.items():
        z = scout.ground_z(location[0] + fx * a + rx * c, location[1] + fy * a + ry * c, z_from=z_from)
        corners[name] = z
    known = sorted(z for z in corners.values() if z is not None)
    if not known:
        return location, rotation
    median = known[len(known) // 2]
    for name, z in corners.items():
        if z is None or abs(z - median) > 120.0:
            corners[name] = median
    front = (corners["fl"] + corners["fr"]) / 2.0
    back = (corners["bl"] + corners["br"]) / 2.0
    left = (corners["fl"] + corners["bl"]) / 2.0
    right = (corners["fr"] + corners["br"]) / 2.0
    pitch = math.degrees(math.atan2(front - back, 2.0 * length))
    # UE roll: positive lowers the right side (right vector of roll +10 has z < 0).
    roll = math.degrees(math.atan2(left - right, 2.0 * width))
    z = sum(corners.values()) / 4.0 - TANK_SINK + extra_z
    return (location[0], location[1], z), (rotation[0] + pitch, rotation[1], rotation[2] + roll)


def add_tank(sequence, spec, seconds):
    """Expands a unit class into its mesh parts; every part follows the unit's keys. With 'snap' the keys are seated
    on the ground under the tracks (pitch and roll included); with 'drive' the tracks and wheels turn."""
    spec = dict(spec)
    footprint = TANK_FOOTPRINT.get(spec["asset"])
    if spec.get("snap") is not None and footprint:
        fitted = []
        for key in _densify(spec["keys"], 12):
            # trace_from: start the traces low where a structure stands over the path (the Ural gateway)
            location, rotation = _ground_fit(key[1], key[2], footprint, key[1][2], spec.get("trace_from", 50000.0))
            fitted.append((key[0], location, rotation) + tuple(key[3:]))
        spec["keys"] = fitted
        spec["snap"] = None
    drive = TANK_DRIVE.get(spec["asset"]) if spec.get("drive") else None
    bindings = []
    # kit_scale: a scale model of the tank (the map-table miniatures of 2.4): offsets and parts shrink together.
    kit_scale = float(spec.get("kit_scale", 1.0))
    for index, entry in enumerate(TANK_KITS[spec["asset"]]):
        kind, asset, offset, materials = entry[:4]
        part_scale = (entry[4] if len(entry) > 4 else 1.0) * kit_scale
        part = dict(spec)
        # Some "SM_" turret and gun assets are skeletal meshes: trust the asset, not the name.
        kind = "skel" if isinstance(unreal.load_asset(asset), unreal.SkeletalMesh) else "mesh"
        part.update({"kind": kind, "asset": asset, "mats": materials, "local_offset": tuple(o * kit_scale for o in offset),
                     "label": "%s_part%d" % (spec.get("label", "Tank"), index)})
        if part_scale != 1.0:
            part["_scale"] = (part_scale, part_scale, part_scale)
        if drive and index == 0:
            part["anim"] = drive[0]
            part["play_rate"] = float(spec["drive"])
            part["slot_mats"] = {slot: _drive_track_material(drive[1], float(spec["drive"])) for slot in drive[2]}
        bindings.append(add_actor(sequence, part, seconds))
    if spec.get("contact") and footprint:
        # A soft dark blob on the ground under the hull (M_Intro_ContactShadow): grounds a tank that stands far from
        # the camera, where the shadow map alone leaves a lit strip under the tracks.
        part = dict(spec)
        part.update({"kind": "mesh", "asset": "/Engine/BasicShapes/Plane", "mats": mats.MAT_DIR + "/M_Intro_ContactShadow",
                     "local_offset": (-12.0, 0.0, TANK_SINK + 2.0), "shadow": False, "anim": None,
                     # reaches ~2 m past the hull and ~1.8 m past each track: the lit strip the shadow-map bias leaves at this distance
                     "_scale": (footprint[0] * 2.0 * 2.2 / 100.0, footprint[1] * 2.0 * 2.8 / 100.0, 1.0),
                     "label": "%s_contact" % spec.get("label", "Tank")})
        bindings.append(add_actor(sequence, part, seconds))
    return bindings


def _densify(keys, steps):
    """Extra keys between moving keys (a move over a crest needs the ground under it, not only at its ends)."""
    if len(keys) < 2:
        return list(keys)
    out = [keys[0]]
    for a, c in zip(keys[:-1], keys[1:]):
        moved = math.hypot(c[1][0] - a[1][0], c[1][1] - a[1][1])
        count = max(1, int(round(steps * (c[0] - a[0]))) if moved > 50.0 else 1)
        for k in range(1, count + 1):
            f = k / float(count)
            location = tuple(a[1][i] + (c[1][i] - a[1][i]) * f for i in range(3))
            rotation = tuple(a[2][i] + (c[2][i] - a[2][i]) * f for i in range(3)) if not isinstance(a[2], dict) else c[2]
            out.append((a[0] + (c[0] - a[0]) * f, location, rotation) + tuple(c[3:]))
    return out


def _drive_track_material(parent_path, play_rate):
    """A track material that pans at a fixed rate (the game sets PlayRate at run time; a film needs it baked)."""
    name = "MI_Intro_TrackDrive_%s_%d" % (parent_path.rsplit("/", 1)[1], int(round(play_rate * 100)))
    return mats.instance(name, unreal.load_asset(parent_path), scalars={"PlayRate": play_rate}).get_path_name()


def add_actor(sequence, spec, seconds):
    """Adds one spawnable. spec: kind, asset, label, keys [(fraction, loc, rot, scale)], optional mats, anim,
    spawn (on, off fractions), snap (cm above the ground under each key; the key z is then an extra offset)."""
    if spec["kind"] == "class" and spec.get("asset") in TANK_KITS:
        return add_tank(sequence, spec, seconds)
    instance = _make_instance(spec)
    if instance is None:
        return None
    binding = sequence.add_spawnable_from_instance(instance)
    actors_api.destroy_actor(instance)
    binding.set_display_name(spec.get("label", spec["kind"]))
    _key_transform(binding, spec["keys"], seconds, spec.get("snap"), spec.get("_scale"), spec.get("snap_at"),
                   spec.get("local_offset"))
    if spec.get("spawn"):
        _key_spawn(binding, spec["spawn"], seconds)
    animation = spec.get("anim")
    if animation and spec["kind"] == "skel":
        track = binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
        section = track.add_section()
        section.set_range(0, b.frames(seconds))
        params = section.get_editor_property("params")
        params.set_editor_property("animation", unreal.load_asset(resolve(animation)))
        if spec.get("play_rate"):
            rate = unreal.MovieSceneTimeWarpVariant()
            rate.set_fixed_play_rate(float(spec["play_rate"]))
            params.set_editor_property("play_rate", rate)
        section.set_editor_property("params", params)
    return binding


def attach(sequence, child, parent, socket, seconds):
    track = child.add_track(unreal.MovieScene3DAttachTrack)
    section = track.add_section()
    section.set_range(0, b.frames(seconds))
    section.set_constraint_binding_id(sequence.get_binding_id(parent))
    section.set_editor_property("attach_socket_name", socket)


def add_mpc(sequence, keys, seconds, archive):
    """Material Parameter Collection keys. keys: {name: [(fraction, value or callable(seconds))]}.
    Every shot states every scalar, so nothing leaks from the shot before."""
    track = sequence.add_track(unreal.MovieSceneMaterialParameterCollectionTrack)
    track.set_editor_property("mpc", unreal.load_asset(mats.MPC_PATH))
    section = track.add_section()
    section.set_range(0, b.frames(seconds))
    values = {name: [(0.0, default)] for name, default in mats.MPC_SCALARS.items()}
    values["Archive"] = [(0.0, 1.0 if archive else 0.0)]
    values.update(keys or {})
    for name, pairs in values.items():
        for fraction, value in pairs:
            if callable(value):
                value = value(seconds)
            # This call takes TICKS (24000 per second), unlike the channel API, which takes display frames.
            section.add_scalar_parameter_key(name, FRAME(int(round(fraction * seconds * TICKS_PER_SECOND))), float(value))


def move_level_actor(sequence, move, seconds):
    """Animates an actor that lives in a streamed map (found by its label) for the length of the shot. Sequencer
    restores it afterwards; nothing is written to the map. move: level, label, keys [(fraction, loc, rot, scale)]."""
    package = "/" + move["level"] + "."
    for actor in actors_api.get_all_level_actors():
        if actor.get_actor_label() == move["label"] and package in actor.get_path_name():
            binding = sequence.add_possessable(actor)
            _key_transform(binding, move["keys"], seconds)
            print("  level actor bound:", move["label"])
            return binding
    print("  level actor NOT found:", move["label"])
    return None


def _handheld(keys, seconds, degrees, seed):
    """Resamples a camera move every third of a second and adds small, smooth, seeded offsets (a held camera)."""
    import random
    rng = random.Random(str(seed))
    step = 0.33
    count = max(2, int(seconds / step))
    out = []
    for index in range(count + 1):
        t = seconds * index / count
        k = 0
        while k < len(keys) - 2 and t > keys[k + 1][0]:
            k += 1
        t0, p0, r0 = keys[k]
        t1, p1, r1 = keys[k + 1]
        f = 0.0 if t1 <= t0 else min(max((t - t0) / (t1 - t0), 0.0), 1.0)
        position = tuple(p0[i] + (p1[i] - p0[i]) * f + rng.uniform(-1.0, 1.0) * degrees * 3.0 for i in range(3))
        rotation = tuple(r0[i] + (r1[i] - r0[i]) * f + rng.uniform(-1.0, 1.0) * degrees * (0.4 if i == 2 else 1.0) for i in range(3))
        out.append((t, position, rotation))
    return out


def build_shot(sequence, shot, seconds):
    scout.set_levels_visible(shot.get("levels", []))
    keys = []
    for key in shot["cam"]:
        position = _ground(key[1], shot.get("cam_snap"))
        rotation = key[2] if not isinstance(key[2], dict) else look_at(position, _ground(key[2]["at"], key[2].get("snap")))
        keys.append((key[0] * seconds, position, rotation))
    # Camera feel: a two-key move runs at constant speed (already moving at the cut, still moving at the next one)
    # unless the shot asks for "ease"; "handheld" (degrees) lays a small restless drift over the move.
    style = shot.get("move", "linear" if len(keys) == 2 else "ease")
    if shot.get("handheld"):
        keys = _handheld(keys, seconds, float(shot["handheld"]), shot["id"])
        style = "ease"
    b.add_camera(sequence, keys, seconds, focal_length=focal_for_fov(shot.get("fov", 60.0)), linear=style == "linear")
    actors = shot.get("actors", [])
    if callable(actors):            # actors that depend on the shot's final length (5.1 canisters)
        actors = actors(seconds)
    by_label = {}
    for spec in actors:
        binding = add_actor(sequence, spec, seconds)
        if spec.get("label") and not isinstance(binding, list):
            by_label[spec["label"]] = binding
    for spec in actors:
        # "attach": (parent label, socket): the actor rides a socket of another actor of the shot (a rifle in a
        # marching soldier's hand); its keys are then relative to that socket.
        if spec.get("attach") and by_label.get(spec["label"]) and by_label.get(spec["attach"][0]):
            attach(sequence, by_label[spec["label"]], by_label[spec["attach"][0]], spec["attach"][1], seconds)
    for move in shot.get("move_level_actors", []):
        move_level_actor(sequence, move, seconds)


def add_level_visibility(master, shots, timeline):
    """One Level Visibility track; per level one section per unbroken run of shots that use it (a level that stays
    on across a cut must not be hidden and shown again at the cut)."""
    runs = {}
    for shot, placed in zip(shots, timeline):
        start, end = b.frames(placed["start"]), b.frames(placed["start"] + placed["duration"])
        for level in shot.get("levels", []):
            level_runs = runs.setdefault(level, [])
            if level_runs and level_runs[-1][1] == start:
                level_runs[-1][1] = end
            else:
                level_runs.append([start, end])
    track = master.add_track(unreal.MovieSceneLevelVisibilityTrack)
    for row, (level, level_runs) in enumerate(sorted(runs.items())):
        for start, end in level_runs:
            section = track.add_section()
            section.set_range(start, end)
            section.set_visibility(unreal.LevelVisibility.VISIBLE)
            section.set_level_names([unreal.Name(level)])
            section.set_row_index(row)


# Clocks that keep their value from shot to shot (what burnt stays burnt); everything else falls back per shot.
STICKY = ("Rings36", "Rings42", "Rings62")


def add_master_mpc(master, shots, timeline):
    """One Material Parameter Collection track on the master for the whole film. Per shot: Archive from the grade,
    every other scalar back to its default unless the shot keys it, and the last value held to the shot's end, so
    no frame at a cut falls between two shots' values."""
    track = master.add_track(unreal.MovieSceneMaterialParameterCollectionTrack)
    track.set_editor_property("mpc", unreal.load_asset(mats.MPC_PATH))
    section = track.add_section()
    total = timeline[-1]["start"] + timeline[-1]["duration"]
    section.set_range(0, b.frames(total))

    def key(name, seconds, value):
        section.add_scalar_parameter_key(name, FRAME(int(round(seconds * TICKS_PER_SECOND))), float(value))

    hold = 1.0 / b.FPS
    held = {name: 0.0 for name in STICKY}      # where each sticky clock stood at the end of the previous shot
    for shot, placed in zip(shots, timeline):
        start, seconds = placed["start"], placed["duration"]
        # Every shot (the reused logo sequence too) starts from the defaults: a flash or blackout never leaks on.
        keys = {name: [(0.0, default)] for name, default in mats.MPC_SCALARS.items() if name not in STICKY}
        keys["Archive"] = [(0.0, 1.0 if shot.get("grade", "archive") == "archive" else 0.0)]
        # "mpc_run_on": sticky clocks that keep running through this shot from where the last shot left them.
        for name in shot.get("mpc_run_on", []):
            keys[name] = [(0.0, held[name]), (1.0, held[name] + seconds)]
        keys.update(shot.get("mpc") or {})
        for name, pairs in keys.items():
            last = None
            for fraction, value in pairs:
                if callable(value):
                    value = value(seconds)
                at = min(fraction * seconds, seconds - hold)
                key(name, start + at, value)
                last = (at, value)
            if name in held:
                held[name] = last[1]
            if last[0] < seconds - hold - 1e-4:
                key(name, start + seconds - hold, last[1])


def build_film(shots, global_cues):
    """Builds every shot sequence, the master, the cue table. Returns the timeline."""
    timeline, total = b.layout(shots)
    master = b.new_sequence(b.MASTER, total)
    shot_track = master.add_track(unreal.MovieSceneCinematicShotTrack)
    voice_track = master.add_track(unreal.MovieSceneAudioTrack)
    voice_track.set_display_name("Narrator")
    packages = [master.get_outermost()]
    cues = list(global_cues)
    voice_index = 0
    for shot, placed in zip(shots, timeline):
        start, seconds = placed["start"], placed["duration"]
        print("shot %-5s %7.2f s  +%6.2f" % (shot["id"], start, seconds))
        if shot.get("sequence"):
            sub = unreal.load_asset(shot["sequence"])
        else:
            sub = b.new_sequence("%s/LS_Intro_%s" % (b.SEQ_DIR, shot["id"].replace(".", "_")), seconds)
            build_shot(sub, shot, seconds)
            packages.append(sub.get_outermost())
        section = shot_track.add_section()
        section.set_sequence(sub)
        section.set_range(b.frames(start), b.frames(start + seconds))
        section.set_shot_display_name(shot["id"])
        if not shot.get("sequence"):
            # The shot's camera and actors exist a few frames before the cut, so its first frame is complete.
            section.set_pre_roll_frames(PRE_ROLL_FRAMES)
        for row in placed["vo"]:
            audio = voice_track.add_section()
            audio.set_sound(unreal.load_asset(b.vo_asset_path(row["row"])))
            audio.set_range(b.frames(row["start"]), b.frames(row["start"] + row["duration"]))
            audio.set_row_index(voice_index % 2)
            voice_index += 1
        for cue in shot.get("cues", []):
            at = cue["at"]
            if "after_vo" in cue:      # seconds after the end of the shot's n-th voice row
                row = placed["vo"][cue["after_vo"]]
                at = row["start"] - start + row["duration"] + cue["at"]
            if at < 0:                 # negative = seconds before the shot's end
                at = seconds + at
            entry = dict(cue)
            entry["start"] = start + at
            # Nothing typed or stamped in a shot outlives it.
            entry["end"] = min(start + at + cue["dur"], start + seconds - 0.05)
            cues.append(entry)
    add_level_visibility(master, shots, timeline)
    add_master_mpc(master, shots, timeline)
    fades = [(placed["start"], shot.get("fade_in", 0.0), placed["start"] + placed["duration"], shot.get("fade_out", 0.0))
             for shot, placed in zip(shots, timeline)]
    b.add_fades(master, fades, total)
    lib.safe_save(packages)
    scout.set_levels_visible([])
    with open(b.TIMELINE_JSON, "w", encoding="utf-8") as handle:
        json.dump({"total": total, "fps": b.FPS, "shots": timeline, "cues": cues}, handle, indent=1)
    b.build_cues(cues)
    print("film %.1f s, %d shots, %d cues" % (total, len(shots), len(cues)))
    return timeline, total


def rebuild_in_place(shots, ids):
    """Rebuilds only these shots' subsequences at their current length (from the timeline of the last full build).
    The master is not touched, so a patch render of the whole film's frame range shows the change. A shot whose
    voice rows changed length needs a full build_film instead."""
    with open(b.TIMELINE_JSON, encoding="utf-8") as handle:
        placed = {entry["id"]: entry for entry in json.load(handle)["shots"]}
    packages = []
    for shot in shots:
        if shot["id"] not in ids or shot.get("sequence"):
            continue
        seconds = placed[shot["id"]]["duration"]
        sub = b.new_sequence("%s/LS_Intro_%s" % (b.SEQ_DIR, shot["id"].replace(".", "_")), seconds)
        build_shot(sub, shot, seconds)
        packages.append(sub.get_outermost())
        print("rebuilt shot", shot["id"], "%.2f s" % seconds)
    lib.safe_save(packages)
    scout.set_levels_visible([])
