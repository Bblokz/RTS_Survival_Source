"""Builds BP_OreLookoutTower (child of ALookoutTowerCaptureActor), headless. Rerunnable; needs the meshes from
ue_lookout_import.py and a DLL with ALookoutTowerCaptureActor (DebugGame until Development is rebuilt):

  sh run_ue_py.sh ue_lookout_bp.py <log> DebugGame

/Game/RTS_Survival/Blueprints/Environment/OreMountains/LookoutTowers/BP_OreLookoutTower, no graph (C++ does it all):
  DefaultSceneRoot (static)
    TowerMesh     SM_OreLookout_Schachtwarte, BlockAll + affects navigation (13 UCX hulls); C++ adds shell/shot blocks
    SheaveAxle    scene component on the axle (0, 0, 1340), tag LookoutSpin (turns about X while held)
      Sheave_N / Sheave_S   SM_OreLookout_Sheave at the Sheave_N / Sheave_S sockets, unrotated, no collision
    HeldPanel     SM_OreLookout_HeldPanel at the HeldPanel socket, unrotated, tag LookoutHeldPanel (hidden until held)
Class defaults: 2 capture units, 20 s, "Manning the lookout...", reveal 9000 cm (C++ defaults; set here explicitly).
"""
import os

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/LookoutTowers'
BP_NAME = 'BP_OreLookoutTower'
MESHES = HOME + '/Meshes/'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
CAPTURE_UNITS = 2
CAPTURE_SECONDS = 20.0
# No click volume over the headframe: ground finding (drops, abilities, decals) traces Visibility too, and a 15 m box
# would make a roof there. The cursor takes the tower by its hulls (footprint to 3.6 m, hoist house walls to 4.2 m).
failures = []


def log(*a):
    unreal.log_warning('LOOKOUT_BP ' + ' '.join(str(x) for x in a))


def fail(msg):
    failures.append(msg)
    log('FAIL', msg)


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        fail('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + \
        path.replace('/Game/', '', 1) + '.uasset'
    if not os.path.isfile(disk):
        fail('not on disk ' + disk)
    else:
        log('saved', disk, os.path.getsize(disk), 'bytes')


def add(blueprint, name, cls, parent_handle):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=blueprint)
    handle, reason = SDS.add_new_subobject(params)
    if not SDL.is_handle_valid(handle):
        raise RuntimeError('add %s failed: %s' % (name, reason))
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle, SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), blueprint)


def place(comp, loc):
    comp.set_editor_property('relative_location', unreal.Vector(*loc))
    comp.set_editor_property('relative_rotation', unreal.Rotator(0.0, 0.0, 0.0))


def visual_mesh(blueprint, name, parent_handle, mesh, loc, mobility, tags=()):
    handle, comp = add(blueprint, name, unreal.StaticMeshComponent, parent_handle)
    comp.set_editor_property('static_mesh', mesh)
    comp.set_editor_property('mobility', mobility)
    place(comp, loc)
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)
    if tags:
        comp.set_editor_property('component_tags', list(tags))
    return handle, comp


def socket_location(mesh, name):
    sock = mesh.find_socket(name)
    if sock is None:
        fail('tower mesh has no socket ' + name)
        return unreal.Vector(0.0, 0.0, 0.0)
    return sock.get_editor_property('relative_location')


def build():
    path = HOME + '/' + BP_NAME
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    parent = getattr(unreal, 'LookoutTowerCaptureActor', None)
    if parent is None:
        fail('ALookoutTowerCaptureActor is not in this DLL (run with DebugGame or rebuild Development)')
        return None
    tower = unreal.load_asset(MESHES + 'SM_OreLookout_Schachtwarte')
    sheave = unreal.load_asset(MESHES + 'SM_OreLookout_Sheave')
    panel = unreal.load_asset(MESHES + 'SM_OreLookout_HeldPanel')
    if None in (tower, sheave, panel):
        fail('meshes missing: run ue_lookout_import.py first')
        return None

    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent)
    blueprint = TOOLS.create_asset(BP_NAME, HOME, unreal.Blueprint, factory)
    handles = SDS.k2_gather_subobject_data_for_blueprint(blueprint)
    root_handle = handles[1] if len(handles) > 1 else handles[0]
    root = SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(root_handle), blueprint)
    root.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    static = unreal.ComponentMobility.STATIC
    movable = unreal.ComponentMobility.MOVABLE

    _, mesh = add(blueprint, 'TowerMesh', unreal.StaticMeshComponent, root_handle)
    mesh.set_editor_property('static_mesh', tower)
    mesh.set_editor_property('mobility', static)
    place(mesh, (0.0, 0.0, 0.0))
    mesh.set_collision_profile_name('BlockAll')
    mesh.set_editor_property('can_ever_affect_navigation', True)

    axle_loc = socket_location(tower, 'Sheave_N')
    axle_loc = unreal.Vector(0.0, 0.0, axle_loc.z)
    axle_handle, axle = add(blueprint, 'SheaveAxle', unreal.SceneComponent, root_handle)
    axle.set_editor_property('mobility', movable)
    place(axle, (axle_loc.x, axle_loc.y, axle_loc.z))
    axle.set_editor_property('component_tags', ['LookoutSpin'])
    for name in ('Sheave_N', 'Sheave_S'):
        s = socket_location(tower, name)
        visual_mesh(blueprint, name, axle_handle, sheave, (s.x - axle_loc.x, s.y - axle_loc.y, s.z - axle_loc.z), movable)
    p = socket_location(tower, 'HeldPanel')
    visual_mesh(blueprint, 'HeldPanel', root_handle, panel, (p.x, p.y, p.z), static, tags=('LookoutHeldPanel',))

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    cs = cdo.get_editor_property('capture_settings')
    cs.set_editor_property('amount_capture_units_needed', CAPTURE_UNITS)
    cs.set_editor_property('capture_duration', CAPTURE_SECONDS)
    cdo.set_editor_property('capture_settings', cs)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(path)
    return blueprint


bp = build()
log('RESULT', 'OK' if bp and not failures else 'FAIL %s' % failures)
