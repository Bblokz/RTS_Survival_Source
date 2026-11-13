"""Forge survey station POI for Ger_Berlin: imports SM_Berlin_ForgeStation (v2, Director of Forge style) and puts the
existing Director of Forge materials on its four slots. No materials are created.

Blender source (generator, validator, previews, report, style note ForgeStyle.md):
    D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinForgeStation/
    (the first, olive field-kit version and its import script are in _v1_fieldkit/ there)
Usage (headless -run=pythonscript wrapper forge_station_import_job.py, or in a throwaway editor):
    exec(open(r'<this file>').read()); result = run()
Nothing runs at import time. Safe to rerun after a Blender rebuild (replaces the mesh).

Asset in /Game/RTS_Survival/Blueprints/Environment/Berlin/ForgeStation:
- SM_Berlin_ForgeStation: one static mesh, 11 UCX box hulls (gate piers, cryo block, stack, bunkers, hut, rig, tanks;
  none over the vehicle lane), LightMapUV in UV1, no auto collision, no Nanite.
  Pivot = ground centre of the 30 x 30 m footprint, +X faces the crater (unchanged from v1).
  Lane (UE frame, Y mirrored from Blender): x -50 .. 700 cm over the full Y length, free of collision. The gate arch
  spans it at y 440 .. 740 cm: opening x -100 .. 750 cm, 497 cm clear at the lowest point over the lane, 620 cm crown.
  Data console desk at (-190, -520, 0), stand point at (-90, -520, 0) (unchanged from v1).
- Slots: M_FS_Concrete -> MI_SovietConcrete_Tilling, M_FS_CamoSteel -> M_Nuclear_Tower_Soviet,
  M_FS_Plant -> MI_CoolingTowers_Soviet, M_FS_Radixite -> MI_Crystal1 (the Radixite crystal material).
  The v1 instances MI_FS_PaintedSteel / _BareSteel / _CryoInsulation / _Canvas in the same folder are no longer used.
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinForgeStation/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/Berlin/ForgeStation'
MESH = 'SM_Berlin_ForgeStation'
EXPECT_HULLS = 11
RUS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'

# slot name in the FBX -> existing Director of Forge material
SLOTS = {
    'M_FS_Concrete': RUS + 'RusBunkers/Materials/MI_SovietConcrete_Tilling',
    'M_FS_CamoSteel': '/Game/RTS_Survival/Meshes/Buildings/Industrial/Textures/M_Nuclear_Tower_Soviet',
    'M_FS_Plant': RUS + 'RusBuildings/CoolingTowersMeshes/Materials/MI_CoolingTowers_Soviet',
    'M_FS_Radixite': '/Game/RTS_Survival/Blueprints/Resources/Crystals/Textures/MI_Crystal1',
}


def _save(obj):
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, False):
        raise RuntimeError('save failed ' + obj.get_path_name())


def import_mesh():
    """Legacy FBX importer: Interchange drops UCX hulls and ignores FbxImportUI."""
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + MESH + '.FBX'
    task.destination_path = HOME
    task.destination_name = MESH
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(f'{HOME}/{MESH}')
    if mesh is None:
        raise RuntimeError('import failed: ' + MODEL + MESH + '.FBX')
    mesh.set_editor_property('light_map_coordinate_index', 1)
    return mesh


def run(save=True):
    """Import the mesh, assign the Forge materials per slot, save. Returns paths, counts, bounds.
    save=False keeps the result in memory only (look captures in a throwaway editor)."""
    if not unreal.EditorAssetLibrary.does_directory_exist(HOME):
        unreal.EditorAssetLibrary.make_directory(HOME)
    mesh = import_mesh()
    assigned = {}
    slots = mesh.get_editor_property('static_materials')
    if len(slots) != len(SLOTS):
        raise RuntimeError(f'expected {len(SLOTS)} slots, got {len(slots)}')
    # The legacy importer names each slot after its FBX material (imported_material_slot_name, read-only) and may sort
    # them in any order, so materials are matched by that name, never by index. If the names are not the four expected
    # ones, stop instead of guessing. A reimport over an older mesh can leave old display names (material_slot_name)
    # behind, so those are rewritten below.
    names = [str(s.get_editor_property('imported_material_slot_name')) for s in slots]
    order = [next((k for k in SLOTS if n.startswith(k)), None) for n in names]
    if None in order:
        raise RuntimeError(f'slots do not carry the expected FBX material names: {names}')
    rebuilt = []
    for slot, key in zip(slots, order):
        mat = unreal.load_asset(SLOTS[key])
        if mat is None:
            raise RuntimeError('missing material ' + SLOTS[key])
        slot.set_editor_property('material_interface', mat)
        slot.set_editor_property('material_slot_name', key)
        rebuilt.append(slot)
        assigned[key] = mat.get_path_name()
    if len(set(order)) != len(SLOTS):
        raise RuntimeError(f'slot names not unique: {order}')
    mesh.set_editor_property('static_materials', rebuilt)
    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    sections = [sm.get_lod_material_slot(mesh, 0, i) for i in range(mesh.get_num_sections(0))]
    if sorted(sections) != list(range(len(SLOTS))):
        raise RuntimeError(f'sections do not map one to one onto the slots: {sections}')
    if save:
        _save(mesh)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    lo = [round(v, 1) for v in (box.min.x, box.min.y, box.min.z)]
    hi = [round(v, 1) for v in (box.max.x, box.max.y, box.max.z)]
    result = {
        'mesh': mesh.get_path_name(),
        'triangles': mesh.get_num_triangles(0),
        'uv_channels': unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0),
        'convex_hulls': hulls,
        'hulls_ok': hulls == EXPECT_HULLS,
        'materials': assigned,
        'slot_order': order,
        'section_slots': sections,
        'bounds_min_cm': lo,
        'bounds_max_cm': hi,
        'size_cm': [round(b - a, 1) for a, b in zip(lo, hi)],
        'lane_x_cm': [-50.0, 700.0],
        'gate_arch_cm': {'y': [440.0, 740.0], 'opening_x': [-100.0, 750.0], 'clear_over_lane': 497.0, 'crown': 620.0},
        'console_desk_cm': [-190.0, -520.0, 0.0],
        'console_stand_cm': [-90.0, -520.0, 0.0],
    }
    print('FORGE_STATION_IMPORT', result)
    return result
