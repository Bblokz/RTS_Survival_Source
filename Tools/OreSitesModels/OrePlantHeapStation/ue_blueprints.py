"""Builds the Base A ore plant and heap station Blueprints, headless and rerunnable (deletes + recreates them).
Run ue_import.py first (or set OREPLANT_IMPORT=1 to import in the same run).

  sh run_ue_py.sh ue_blueprints.py bps            (Development commandlet, null RHI)

/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/BaseA/
  BP_OreBaseA_CrusherHouse   child of Rus_Building_AmmoStorage (health, owner, death graph, BXP type kept);
                             BuildingMeshComponent = SM_OreBaseA_CrusherHouse; widget / decal / selection box resized
  BP_OreBaseA_SiloGroup      child of Rus_Building_AmmoStorage; BuildingMeshComponent = SM_OreBaseA_SiloTable (table,
                             bridge, tower, gallery to the crusher; its UCX also wraps the silos) + 3 x the project's
                             PostApo SM_Scorch_Silo2 (the BP_Dest_PostApo_Silo2 mesh; scale 1.15, NoCollision, no nav) on the table
  BP_OreBaseA_NissenShed     plain Actor, SM_OreBaseA_NissenShed (BlockAll, affects nav): non-destroyable dressing
/Game/RTS_Survival/Blueprints/Environment/OreMountains/HeapStations/
  BP_OreHeapStation_Erzrolle child of BP_OreHeapStation (capture, drop-off at +8 m X toward the heap, 2500 knock-out
                             kept): StationMesh = SM_OreHeap_Erzrolle at the pivot; Chute (tag HeapStationIntact) and
                             ChuteBroken (tag HeapStationKnockedOut, hidden until knocked out); HeldPanel moved onto the
                             bin, a second HeldPanel_Back on the far side; inherited Rubble kept.
Report: Saved/OreMountains/sites/ue_oreplant/bp_report.json
"""
import json
import os

import unreal

SITES = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
BASEA = SITES + 'Sites/BaseA'
HEAP = SITES + 'HeapStations'
PARENT_A = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_AmmoStorage'
PARENT_H = HEAP + '/BP_OreHeapStation'
SILO_MESH = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/Meshes/SM_Scorch_Silo2'
SILO_SCALE = 1.15
SILO_OFFSETS = [(0.0, -550.0, 380.0), (0.0, 0.0, 380.0), (0.0, 550.0, 380.0)]   # (X, Y, Z) cm, table deck at 3.8 m
PANEL_MESH = SITES + 'LookoutTowers/Meshes/SM_OreLookout_HeldPanel'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/ue_oreplant/bp_report.json'

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
BEL = unreal.BlueprintEditorLibrary
failures = []
report = dict(bps={}, failures=failures)

# per Base A child: building mesh, health bar height (cm), decal scale (AmmoStorage: (30, 500, 410) for a
# 7.05 x 5.68 m footprint, i.e. ~0.71 x the X / Y extent), selection box centre + half size (cm)
BASEA_BPS = {
    'BP_OreBaseA_CrusherHouse': dict(mesh=BASEA + '/Meshes/SM_OreBaseA_CrusherHouse', widget_z=2150.0,
                                     decal=(30.0, 780.0, 900.0), sel_c=(0.0, 150.0, 800.0), sel_h=(560.0, 700.0, 800.0),
                                     silos=False),
    'BP_OreBaseA_SiloGroup': dict(mesh=BASEA + '/Meshes/SM_OreBaseA_SiloTable', widget_z=2250.0,
                                  decal=(30.0, 480.0, 1300.0), sel_c=(-80.0, -100.0, 900.0), sel_h=(420.0, 960.0, 900.0),
                                  silos=True),
}


def log(*a):
    unreal.log_warning('OREPLANT_BP ' + ' '.join(str(x) for x in a))


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        failures.append('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + \
        path.replace('/Game/', '', 1) + '.uasset'
    if not os.path.isfile(disk):
        failures.append('not on disk ' + disk)
    return disk


def new_bp(name, home, parent_class):
    path = home + '/' + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    f = unreal.BlueprintFactory()
    f.set_editor_property('parent_class', parent_class)
    return TOOLS.create_asset(name, home, unreal.Blueprint, f), path


def handles(bp):
    out = {}
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDS.k2_find_subobject_data_from_handle(h)
        o = SDL.get_object_for_blueprint(data, bp)
        if o is not None:
            out[o.get_name()] = (h, o)
    return out


def find(bp, prefix):
    for n, (h, o) in handles(bp).items():
        if n.startswith(prefix):
            return h, o
    return None, None


def add_comp(bp, name, cls, parent_handle):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=bp)
    h, reason = SDS.add_new_subobject(params)
    if not SDL.is_handle_valid(h):
        raise RuntimeError('add %s failed: %s' % (name, reason))
    SDS.rename_subobject(h, unreal.Text(name))
    return h, SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(h), bp)


def visual(comp, mesh, loc, rot=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0), tags=(), visible=True):
    comp.set_editor_property('static_mesh', mesh)
    comp.set_editor_property('relative_location', unreal.Vector(*loc))
    comp.set_editor_property('relative_rotation', unreal.Rotator(*rot))
    comp.set_editor_property('relative_scale3d', unreal.Vector(*scale))
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('generate_overlap_events', False)
    comp.set_editor_property('component_tags', [unreal.Name(t) for t in tags])
    comp.set_editor_property('visible', visible)


def build_base_a(name, cfg):
    parent = unreal.EditorAssetLibrary.load_blueprint_class(PARENT_A)
    bp, path = new_bp(name, BASEA, parent)
    mesh = unreal.load_asset(cfg['mesh'])
    if mesh is None:
        failures.append('%s: mesh missing %s' % (name, cfg['mesh']))
        return
    entry = dict(asset=path, parent=PARENT_A, mesh=cfg['mesh'])
    # inherited Blueprint components of the AmmoStorage: overrides are stored in this child (inheritable handler)
    _, widget = find(bp, 'ProgressBarWidget')
    if widget:
        loc = widget.get_editor_property('relative_location')
        widget.set_editor_property('relative_location', unreal.Vector(loc.x, loc.y, cfg['widget_z']))
    else:
        failures.append(name + ': ProgressBarWidget not found')
    _, decal = find(bp, 'SelectedDecal')
    if decal:
        decal.set_editor_property('relative_scale3d', unreal.Vector(*cfg['decal']))
    else:
        failures.append(name + ': SelectedDecal not found')
    _, box = find(bp, 'bpSelectionArea')
    if box:
        ext = box.get_editor_property('box_extent')
        entry['selection_box_extent'] = [ext.x, ext.y, ext.z]
        box.set_editor_property('relative_location', unreal.Vector(*cfg['sel_c']))
        box.set_editor_property('relative_scale3d', unreal.Vector(cfg['sel_h'][0] / max(ext.x, 1.0),
                                                                   cfg['sel_h'][1] / max(ext.y, 1.0),
                                                                   cfg['sel_h'][2] / max(ext.z, 1.0)))
    else:
        failures.append(name + ': bpSelectionArea not found')
    if cfg['silos']:
        root_h, _ = find(bp, 'BuildingMeshComponent')
        silo = unreal.load_asset(SILO_MESH)
        if root_h is None or silo is None:
            failures.append(name + ': no root handle or silo mesh')
        else:
            for label, off in zip(('Silo_W', 'Silo_C', 'Silo_E'), SILO_OFFSETS):
                _, c = add_comp(bp, label, unreal.StaticMeshComponent, root_h)
                visual(c, silo, off, scale=(SILO_SCALE,) * 3)
            entry['silos'] = dict(mesh=SILO_MESH, scale=SILO_SCALE, offsets_cm=SILO_OFFSETS)
    BEL.compile_blueprint(bp)
    # C++ component of ABuildingExpansion (root): set on this class's CDO
    cdo = unreal.get_default_object(bp.generated_class())
    bmc = cdo.get_editor_property('building_mesh_component')
    bmc.set_editor_property('static_mesh', mesh)
    BEL.compile_blueprint(bp)
    entry['disk'] = save(path)
    report['bps'][name] = entry
    log(name, 'built')


def build_shed():
    bp, path = new_bp('BP_OreBaseA_NissenShed', BASEA, unreal.Actor)
    root_h = SDS.k2_gather_subobject_data_for_blueprint(bp)[0]
    h, c = add_comp(bp, 'ShedMesh', unreal.StaticMeshComponent, root_h)
    c.set_editor_property('static_mesh', unreal.load_asset(BASEA + '/Meshes/SM_OreBaseA_NissenShed'))
    c.set_collision_profile_name('BlockAll')
    c.set_editor_property('can_ever_affect_navigation', True)
    BEL.compile_blueprint(bp)
    report['bps']['BP_OreBaseA_NissenShed'] = dict(asset=path, disk=save(path))
    log('BP_OreBaseA_NissenShed built')


def build_heap():
    parent = unreal.EditorAssetLibrary.load_blueprint_class(PARENT_H)
    if parent is None:
        failures.append('BP_OreHeapStation class not loadable (needs AOreHeapStation in this DLL)')
        return
    bp, path = new_bp('BP_OreHeapStation_Erzrolle', HEAP, parent)
    mesh = unreal.load_asset(HEAP + '/Meshes/SM_OreHeap_Erzrolle')
    chute = unreal.load_asset(HEAP + '/Meshes/SM_OreHeap_ErzrolleChute')
    broken = unreal.load_asset(HEAP + '/Meshes/SM_OreHeap_ErzrolleChuteBroken')
    panel = unreal.load_asset(PANEL_MESH)
    root_h, _ = find(bp, 'StationRoot')
    if None in (mesh, chute, broken, panel) or root_h is None:
        failures.append('heap: missing %s root=%s' % ([mesh, chute, broken, panel], root_h))
        return
    _, c = add_comp(bp, 'Chute', unreal.StaticMeshComponent, root_h)
    visual(c, chute, (0.0, 0.0, 0.0), tags=('HeapStationIntact',))
    _, c = add_comp(bp, 'ChuteBroken', unreal.StaticMeshComponent, root_h)
    visual(c, broken, (0.0, 0.0, 0.0), tags=('HeapStationKnockedOut',), visible=False)
    _, c = add_comp(bp, 'HeldPanel_Back', unreal.StaticMeshComponent, root_h)
    visual(c, panel, (-250.0, 60.0, 600.0), rot=(0.0, 0.0, 180.0), tags=('HeapStationHeld',))
    _, held = find(bp, 'HeldPanel')
    if held:
        held.set_editor_property('relative_location', unreal.Vector(300.0, -120.0, 625.0))
    else:
        failures.append('heap: inherited HeldPanel not found')
    BEL.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    sm = cdo.get_editor_property('station_mesh')
    sm.set_editor_property('static_mesh', mesh)
    sm.set_editor_property('relative_location', unreal.Vector(0.0, 0.0, 0.0))
    sm.set_collision_profile_name('BlockAll')
    sm.set_editor_property('can_ever_affect_navigation', True)
    BEL.compile_blueprint(bp)
    report['bps']['BP_OreHeapStation_Erzrolle'] = dict(asset=path, parent=PARENT_H, disk=save(path))
    log('BP_OreHeapStation_Erzrolle built')


def main():
    if os.environ.get('OREPLANT_IMPORT') == '1':
        ns = {}
        exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/OreSitesModels/OrePlantHeapStation/ue_import.py').read(), ns)
        report['import_failures'] = ns['failures']
    for name, cfg in BASEA_BPS.items():
        try:
            build_base_a(name, cfg)
        except Exception as e:  # noqa: BLE001
            failures.append('%s: %r' % (name, e))
    for fn in (build_shed, build_heap):
        try:
            fn()
        except Exception as e:  # noqa: BLE001
            failures.append('%s: %r' % (fn.__name__, e))
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as f:
        json.dump(report, f, indent=1, default=str)
    log('RESULT', 'OK' if not failures else 'FAIL %s' % failures)


main()
