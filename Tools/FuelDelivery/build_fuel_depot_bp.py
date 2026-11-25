# Builds Rus_BXP_LongCamoBunker_FuelDepot: a child of Rus_BXP_LongCamoBunker with a UFuelDeliveryDepotComponent and its
# three UFuelDeliveryNodeComponents (truck spawn = orange, truck park = green, crane = cyan; editor-only arrows).
# The node offsets copy the Ger_IntroMission layout of the Secure the Line fuel delivery, relative to the long bunker
# there: crane west of the bunker inside the wall, truck parked parallel outside the east wall, spawn 35 m south.
# Re-runnable: an existing Blueprint is reused and its nodes are re-set. Does not touch any map.
# Headless (DebugGame): UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
import math

import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers'
NAME = 'Rus_BXP_LongCamoBunker_FuelDepot'
PARENT = FOLDER + '/Rus_BXP_LongCamoBunker.Rus_BXP_LongCamoBunker_C'
INTRO_MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission'
TRUCK = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilCargo/'
         'Rus_ArmoredCar_ZilFuel.Rus_ArmoredCar_ZilFuel_C')
CRANE = '/Game/RTS_Survival/Blueprints/Missions/MissionProps/FuelCrane/BP_MissionFuelCrane.BP_MissionFuelCrane_C'

# World layout on Ger_IntroMission (Source/Tools/IntroMission/setup_secure_line_mission.py + MissionFuelCrane.md):
# the truck stood at (4640, -1717) after its order to (4640, -1580); it spawned at (4640, -5200, 850).
WORLD_NODES = {
    'TruckSpawn': ((4640.0, -5200.0, 850.0), 90.0),
    'TruckPark': ((4640.0, -1717.0, 531.0), 90.0),
    'Crane': ((3650.0, -1850.0, 532.0), 0.0),
}
# Used when the bunker cannot be found on the intro map (bunker-local offsets, bunker at yaw 0).
FALLBACK_BUNKER = ((4150.0, -1850.0, 532.0), 0.0)

NODES = [
    # name, role enum, colour, label
    ('FuelTruckSpawnNode', unreal.FuelDeliveryNodeRole.TRUCK_SPAWN, unreal.LinearColor(1.0, 0.35, 0.0, 1.0),
     'TruckSpawn'),
    ('FuelTruckParkNode', unreal.FuelDeliveryNodeRole.TRUCK_PARK, unreal.LinearColor(0.1, 1.0, 0.2, 1.0),
     'TruckPark'),
    ('FuelCraneNode', unreal.FuelDeliveryNodeRole.CRANE, unreal.LinearColor(0.0, 0.85, 1.0, 1.0), 'Crane'),
]


def log(text):
    unreal.log_warning('RTS_FUELDEPOT_BUILD ' + text)


def find_intro_bunker():
    """World transform (location, yaw) of the long camo bunker on the intro map, or None."""
    world = unreal.load_asset(INTRO_MAP)
    if world is None:
        log('intro map did not load')
        return None
    found = []
    for actor in unreal.ObjectIterator(unreal.Actor):
        if actor.get_class().get_name() != 'Rus_BXP_LongCamoBunker_C':
            continue
        if not actor.get_path_name().startswith(INTRO_MAP + '.'):
            continue
        # The map's world is not initialised in a commandlet, so the actor transform is not computed yet: read the
        # root component's saved (relative = world, unattached) transform.
        root = actor.get_editor_property('root_component')
        location = root.get_editor_property('relative_location')
        yaw = root.get_editor_property('relative_rotation').yaw
        found.append(((location.x, location.y, location.z), yaw, actor.get_name()))
    log('intro bunkers: %s' % found)
    if not found:
        return None
    crane_xy = WORLD_NODES['Crane'][0]
    # The one next to the crane (there may be other long bunkers on the map).
    found.sort(key=lambda entry: math.hypot(entry[0][0] - crane_xy[0], entry[0][1] - crane_xy[1]))
    return found[0][0], found[0][1]


def to_local(bunker, world_location, world_yaw):
    (bx, by, bz), byaw = bunker
    dx, dy, dz = world_location[0] - bx, world_location[1] - by, world_location[2] - bz
    rad = math.radians(-byaw)
    lx = dx * math.cos(rad) - dy * math.sin(rad)
    ly = dx * math.sin(rad) + dy * math.cos(rad)
    return unreal.Vector(lx, ly, dz), world_yaw - byaw


def get_or_create_blueprint():
    path = FOLDER + '/' + NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path), False
    parent = unreal.load_class(None, PARENT)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Blueprint, factory)
    return blueprint, True


def gather(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    result = {}
    root_handle = None
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None:
            continue
        result[obj.get_name().replace('_GEN_VARIABLE', '')] = (handle, obj)
        if root_handle is None and unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(data):
            root_handle = handle
    return subsystem, result, root_handle


def ensure_component(blueprint, name, component_class, attach_to_root):
    subsystem, components, root_handle = gather(blueprint)
    if name in components:
        return components[name][1]
    parent_handle = root_handle if attach_to_root else subsystem.k2_gather_subobject_data_for_blueprint(blueprint)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=component_class,
                                          blueprint_context=blueprint)
    handle, fail_reason = subsystem.add_new_subobject(params)
    if not fail_reason.is_empty():
        raise RuntimeError('could not add %s: %s' % (name, fail_reason))
    subsystem.rename_subobject(handle, unreal.Text(name))
    _, components, _ = gather(blueprint)
    if name not in components:
        raise RuntimeError('added %s but cannot find it (have %s)' % (name, sorted(components)))
    return components[name][1]


def main():
    bunker = find_intro_bunker()
    if bunker is None:
        log('using the fallback bunker transform %s' % (FALLBACK_BUNKER,))
        bunker = FALLBACK_BUNKER
    blueprint, created = get_or_create_blueprint()
    log('blueprint %s created=%s' % (blueprint.get_path_name(), created))
    depot = ensure_component(blueprint, 'FuelDeliveryDepot', unreal.FuelDeliveryDepotComponent, False)
    # Not native defaults: only values saved on the Blueprint are followed by the cooker.
    depot.set_editor_property('truck_class', unreal.load_class(None, TRUCK))
    depot.set_editor_property('crane_class', unreal.load_class(None, CRANE))
    log('depot truck=%s crane=%s' % (depot.get_editor_property('truck_class'),
                                     depot.get_editor_property('crane_class')))
    for name, role, colour, key in NODES:
        node = ensure_component(blueprint, name, unreal.FuelDeliveryNodeComponent, True)
        location, yaw = to_local(bunker, *WORLD_NODES[key])
        node.set_editor_property('node_role', role)
        node.set_editor_property('node_color', colour)
        node.set_editor_property('relative_location', location)
        node.set_editor_property('relative_rotation', unreal.Rotator(0.0, 0.0, yaw))
        log('node %s role=%s local=(%.0f, %.0f, %.0f) yaw=%.0f' % (name, role, location.x, location.y, location.z,
                                                                   yaw))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    _, components, _ = gather(blueprint)
    log('saved=%s components=%s' % (saved, sorted(components)))


main()
