"""Imports the Berlin ruin kit E heroes (3 meshes) for Ger_Berlin.

Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<wrapper that imports this module and calls run()>
or run() in an editor.

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/E_Heroes
  (generate_e_heroes.py; cm data, +X street front, pivot at the footprint centre on the ground (E3: mid-length on the
  row's centre line, the row runs along Y), UCX hulls, colour attribute R soot / G plaster loss / B dust exported
  linear). Contract: BerlinRuinKit/KIT_CONTRACT.md.
Destination: /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Meshes
  SM_BerlinRuin_E1_CornerTower, SM_BerlinRuin_E2_Courtyard, SM_BerlinRuin_E3_CoreRow

Legacy FBX importer (Interchange drops UCX hulls and ignores FbxImportUI), vertex colours replaced from the FBX, UV1
from Blender used as the lightmap channel. Materials by SLOT NAME: M_BerlinRuin_Wall -> MI_BerlinRuin_Wall (slot 0),
M_BerlinRuin_Rubble -> MI_BerlinRuin_Rubble (slot 1); a missing instance leaves its slot empty and is reported. The slot
order and the hull count are checked against the Blender build (e_heroes_build.json) and a mismatch raises. Only the
three assets above are created or saved.
"""
import json
import os

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/E_Heroes/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/'
HOME = ROOT + 'Meshes'
MATS = ROOT + 'Materials/'
SLOT_TO_INSTANCE = {'M_BerlinRuin_Wall': 'MI_BerlinRuin_Wall', 'M_BerlinRuin_Rubble': 'MI_BerlinRuin_Rubble'}
EXPECTED_SLOTS = ['M_BerlinRuin_Wall', 'M_BerlinRuin_Rubble']
MESHES = ('SM_BerlinRuin_E1_CornerTower', 'SM_BerlinRuin_E2_Courtyard', 'SM_BerlinRuin_E3_CoreRow')


def build_report():
    path = MODEL + 'e_heroes_build.json'
    if not os.path.exists(path):
        return {}
    with open(path) as fh:
        return json.load(fh)


def import_mesh(name):
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
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def finish_mesh(name, expected):
    mesh = unreal.load_asset(HOME + '/' + name)
    if mesh is None:
        raise RuntimeError('import made nothing for ' + name)
    slots = mesh.get_editor_property('static_materials')
    slot_names = [str(s.get_editor_property('material_slot_name')) for s in slots]
    if slot_names != EXPECTED_SLOTS:
        raise RuntimeError(f'{name}: slot order {slot_names}, expected {EXPECTED_SLOTS}')
    assigned, missing = [], []
    for index, slot_name in enumerate(slot_names):
        inst_path = MATS + SLOT_TO_INSTANCE[slot_name]
        inst = unreal.load_asset(inst_path) if unreal.EditorAssetLibrary.does_asset_exist(inst_path) else None
        if inst is not None:
            mesh.set_material(index, inst)
            assigned.append(inst.get_name())
        else:
            missing.append(SLOT_TO_INSTANCE[slot_name])
    mesh.set_editor_property('light_map_coordinate_index', 1)
    mesh.modify()
    saved = unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    want_hulls = expected.get('hulls')
    if want_hulls is not None and hulls != want_hulls:
        raise RuntimeError(f'{name}: {hulls} convex hulls after import, the Blender build has {want_hulls}')
    if hulls == 0:
        raise RuntimeError(f'{name}: no UCX hulls arrived (legacy importer not used?)')
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    try:
        has_vc = bool(sms.has_vertex_colors(mesh))
    except Exception:  # noqa: BLE001  (older API names)
        has_vc = None
    rel = '/Content/' + HOME[len('/Game/'):] + '/' + name + '.uasset'
    on_disk = os.path.exists(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()) + rel.lstrip('/'))
    info = {'mesh': mesh.get_path_name(), 'convex': hulls, 'saved': bool(saved), 'uasset_on_disk': on_disk,
            'bounds': [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
            'tris_blender': expected.get('tris'), 'verts': sms.get_number_verts(mesh, 0), 'slots': slot_names,
            'materials': [(mesh.get_material(i).get_name() if mesh.get_material(i) else None) for i in range(len(slots))],
            'missing_instances': missing, 'vertex_colors': has_vc, 'uv_channels': sms.get_num_uv_channels(mesh, 0),
            'light_map_coordinate_index': mesh.get_editor_property('light_map_coordinate_index')}
    unreal.log_warning('BERLIN_RUINKIT_E_HEROES ' + json.dumps(info))
    return info


def run():
    expected = build_report()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in MESHES:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    return [finish_mesh(name, expected.get(name, {})) for name in MESHES]


if __name__ == '__main__':
    run()
