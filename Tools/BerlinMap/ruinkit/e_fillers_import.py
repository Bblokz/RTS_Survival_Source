"""Imports the Berlin ruin kit E fillers (8 meshes) and the kit's six debris pieces for Ger_Berlin.

Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<wrapper that imports this module and calls run()>
or run() in an editor.

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/E_Fillers/FBX
  (generate_e_fillers.py; cm data, +X street front, pivot at the footprint centre on the ground, UCX hulls on the fillers,
  colour attribute R soot / G plaster loss / B dust exported linear). Contract: BerlinRuinKit/KIT_CONTRACT.md.
Destinations:
  /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Meshes   SM_BerlinRuin_E4..E7_*_A/_B
  /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Debris   SM_BerlinRuin_Debris_0..5 (no collision)

Legacy FBX importer (Interchange drops UCX hulls and ignores FbxImportUI), vertex colours replaced from the FBX.
Materials by SLOT NAME: M_BerlinRuin_Wall -> MI_BerlinRuin_Wall (slot 0), M_BerlinRuin_Rubble -> MI_BerlinRuin_Rubble
(slot 1). The slot order is checked and an unexpected order raises. An instance that does not exist yet leaves its slot
empty and is reported. Only assets inside the Meshes and Debris folders are created or saved.
"""
import json

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/E_Fillers/FBX/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/'
MESH_HOME = ROOT + 'Meshes'
DEBRIS_HOME = ROOT + 'Debris'
MATS = ROOT + 'Materials/'
SLOT_TO_INSTANCE = {'M_BerlinRuin_Wall': 'MI_BerlinRuin_Wall', 'M_BerlinRuin_Rubble': 'MI_BerlinRuin_Rubble'}
FILLERS = tuple(f'SM_BerlinRuin_{base}_{v}' for base in ('E4_StubRing', 'E5_Shell1St', 'E6_HeapFin', 'E7_HalfHouse')
                for v in 'AB')
DEBRIS = tuple(f'SM_BerlinRuin_Debris_{i}' for i in range(6))
EXPECTED_SLOTS = {'filler': ['M_BerlinRuin_Wall', 'M_BerlinRuin_Rubble'], 'debris': ['M_BerlinRuin_Wall']}


def import_mesh(name, home):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)      # fillers bring UCX hulls, debris gets none
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)       # UV1 is a packed unwrap from Blender
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = home
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def finish_mesh(name, home, kind):
    mesh = unreal.load_asset(home + '/' + name)
    if mesh is None:
        raise RuntimeError('import made nothing for ' + name)
    slots = mesh.get_editor_property('static_materials')
    slot_names = [str(s.get_editor_property('material_slot_name')) for s in slots]
    if slot_names != EXPECTED_SLOTS[kind]:
        raise RuntimeError(f'{name}: slot order {slot_names}, expected {EXPECTED_SLOTS[kind]}')
    assigned, missing = [], []
    for index, slot_name in enumerate(slot_names):
        inst_name = SLOT_TO_INSTANCE[slot_name]
        inst = unreal.load_asset(MATS + inst_name) if unreal.EditorAssetLibrary.does_asset_exist(MATS + inst_name) else None
        if inst is not None:
            mesh.set_material(index, inst)
            assigned.append(inst_name)
        else:
            missing.append(inst_name)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    mesh.modify()
    unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    try:
        has_vc = bool(sms.has_vertex_colors(mesh))
    except Exception:  # noqa: BLE001  (older API names)
        has_vc = None
    info = {'mesh': mesh.get_path_name(), 'convex': hulls,
            'bounds': [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
            'verts': sms.get_number_verts(mesh, 0), 'slots': slot_names,
            'materials': [(mesh.get_material(i).get_name() if mesh.get_material(i) else None) for i in range(len(slots))],
            'missing_instances': missing, 'vertex_colors': has_vc, 'uv_channels': sms.get_num_uv_channels(mesh, 0)}
    if kind == 'debris' and hulls:
        raise RuntimeError(f'{name}: debris must have no collision, got {hulls} hulls')
    unreal.log_warning('BERLIN_RUINKIT_E ' + json.dumps(info))
    return info


def run():
    jobs = [(n, MESH_HOME, 'filler') for n in FILLERS] + [(n, DEBRIS_HOME, 'debris') for n in DEBRIS]
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name, home, _kind in jobs:
            import_mesh(name, home)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    return [finish_mesh(name, home, kind) for name, home, kind in jobs]


if __name__ == '__main__':
    run()
