"""Builds BP_OreHeapStation (child of the C++ AOreHeapStation), headless and rerunnable. No graph: C++ does it all.

Needs a DLL with AOreHeapStation (DebugGame until Development is rebuilt):
    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended ...

/Game/RTS_Survival/Blueprints/Environment/OreMountains/HeapStations/BP_OreHeapStation
  StationRoot (C++)
    StationMesh (C++)    placeholder ore bin QuarryEnv SM_Silo_2 (10.7 x 10.7 x 7.7 m), centred on the pivot, BlockAll,
                         affects navigation; C++ adds shell and every-player weapon blocks
    DropOffPoint (C++)   harvester unload point, 8 m in front (+X), no mesh, no collision
    HeldPanel            SM_OreLookout_HeldPanel (orange tarp) on the bin's front, tag HeapStationHeld (shown while held)
    Rubble               SM_A4Viaduct_RubbleFan_C at the foot, tag HeapStationKnockedOut (shown once knocked out)
Placeholder look until the owner picks a concept (Saved/OreMountains/sites/v01, sheet_4_heapstation).
Report: Saved/OreMountains/exploration/heap_station_bp.json
"""
import json
import os

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/HeapStations'
BP_NAME = 'BP_OreHeapStation'
BIN_MESH = '/Game/QuarryEnv/Meshes/SM_Silo_2'
PANEL_MESH = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/LookoutTowers/Meshes/SM_OreLookout_HeldPanel'
RUBBLE_MESH = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/A4Viaduct/SM_A4Viaduct_RubbleFan_C'
DROP_OFF_POINT = (800.0, 0.0, 0.0)
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/exploration/heap_station_bp.json'
# Starter values for the owner to tune (C++ defaults, set here explicitly so the Blueprint shows them).
CAPTURE_UNITS = 2
CAPTURE_SECONDS = 20.0
STORAGE = 600
HELD_HEALTH = 2500.0

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
failures = []
report = {}


def log(*a):
    unreal.log_warning('HEAP_BP ' + ' '.join(str(x) for x in a))


def fail(msg):
    failures.append(msg)
    log('FAIL', msg)


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        fail('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + \
        path.replace('/Game/', '', 1) + '.uasset'
    report['disk'] = disk
    report['disk_bytes'] = os.path.getsize(disk) if os.path.isfile(disk) else 0
    if not os.path.isfile(disk):
        fail('not on disk ' + disk)


def add(blueprint, name, cls, parent_handle):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=blueprint)
    handle, reason = SDS.add_new_subobject(params)
    if not SDL.is_handle_valid(handle):
        raise RuntimeError('add %s failed: %s' % (name, reason))
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle, SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), blueprint)


def find_handle(blueprint, name):
    for handle in SDS.k2_gather_subobject_data_for_blueprint(blueprint):
        obj = SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), blueprint)
        if obj is not None and obj.get_name().startswith(name):
            return handle, obj
    return None, None


def visual(blueprint, name, parent_handle, mesh, loc, rot, tag):
    _, comp = add(blueprint, name, unreal.StaticMeshComponent, parent_handle)
    comp.set_editor_property('static_mesh', mesh)
    comp.set_editor_property('relative_location', unreal.Vector(*loc))
    comp.set_editor_property('relative_rotation', unreal.Rotator(*rot))
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)
    comp.set_editor_property('component_tags', [tag])
    return comp


def build():
    path = HOME + '/' + BP_NAME
    parent = getattr(unreal, 'OreHeapStation', None)
    if parent is None:
        fail('AOreHeapStation is not in this DLL (run the DebugGame commandlet or rebuild Development)')
        return None
    meshes = [unreal.load_asset(p) for p in (BIN_MESH, PANEL_MESH, RUBBLE_MESH)]
    if None in meshes:
        fail('stand-in mesh missing %s' % meshes)
        return None
    bin_mesh, panel_mesh, rubble_mesh = meshes
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent)
    blueprint = TOOLS.create_asset(BP_NAME, HOME, unreal.Blueprint, factory)

    root_handle, _ = find_handle(blueprint, 'StationRoot')
    if root_handle is None:
        fail('StationRoot not found')
        return None
    # The bin's pivot sits on a corner: centre it on the station pivot.
    b = bin_mesh.get_bounding_box()
    bin_offset = (-(b.min.x + b.max.x) / 2.0, -(b.min.y + b.max.y) / 2.0, 0.0)
    front_x = (b.max.x - b.min.x) / 2.0
    visual(blueprint, 'HeldPanel', root_handle, panel_mesh, (front_x + 15.0, 0.0, 520.0), (0.0, 0.0, 0.0),
           'HeapStationHeld')
    visual(blueprint, 'Rubble', root_handle, rubble_mesh, (front_x - 150.0, 250.0, -10.0), (0.0, 20.0, 0.0),
           'HeapStationKnockedOut')
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    station_mesh = cdo.get_editor_property('station_mesh')
    station_mesh.set_editor_property('static_mesh', bin_mesh)
    station_mesh.set_editor_property('relative_location', unreal.Vector(*bin_offset))
    station_mesh.set_collision_profile_name('BlockAll')
    station_mesh.set_editor_property('can_ever_affect_navigation', True)
    drop_point = cdo.get_editor_property('drop_off_point')
    drop_point.set_editor_property('relative_location', unreal.Vector(*DROP_OFF_POINT))
    cs = cdo.get_editor_property('capture_settings')
    cs.set_editor_property('amount_capture_units_needed', CAPTURE_UNITS)
    cs.set_editor_property('capture_duration', CAPTURE_SECONDS)
    cdo.set_editor_property('capture_settings', cs)
    ss = cdo.get_editor_property('station_settings')
    ss.set_editor_property('storage_capacity', STORAGE)
    ss.set_editor_property('held_health', HELD_HEALTH)
    cdo.set_editor_property('station_settings', ss)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(path)
    report.update({'bin_offset': bin_offset, 'front_x': front_x,
                   'bin_bounds': [b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z]})
    return blueprint


def check(path):
    """Fresh read of the saved defaults (same process; a separate load is the placement test)."""
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    sm = cdo.get_editor_property('station_mesh')
    rts = cdo.get_editor_property('station_rts_component')
    report['check'] = {
        'parent': unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_name()
        if hasattr(unreal.BlueprintEditorLibrary, 'get_blueprint_parent_class') else 'n/a',
        'mesh': sm.get_editor_property('static_mesh').get_path_name() if sm.get_editor_property('static_mesh') else None,
        'mesh_collision': str(sm.get_collision_profile_name()),
        'mesh_nav': sm.get_editor_property('can_ever_affect_navigation'),
        'owner': rts.get_owning_player() if rts else None,
        'capture_units': cdo.get_editor_property('capture_settings').get_editor_property('amount_capture_units_needed'),
        'storage': cdo.get_editor_property('station_settings').get_editor_property('storage_capacity'),
    }
    if report['check']['mesh'] is None or report['check']['owner'] != 0:
        fail('check %s' % report['check'])


if build() is not None:
    check(HOME + '/' + BP_NAME)
report['failures'] = failures
os.makedirs(os.path.dirname(REPORT), exist_ok=True)
with open(REPORT, 'w') as handle:
    json.dump(report, handle, indent=1, default=str)
log('RESULT', 'OK' if not failures else 'FAIL %s' % failures)
