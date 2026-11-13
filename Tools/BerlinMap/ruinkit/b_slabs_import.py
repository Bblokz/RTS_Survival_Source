"""Imports the Berlin ruin kit B slabs ("Mauerscheiben", 11 meshes) for Ger_Berlin.

Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<wrapper that imports this module and calls run()>
or run() in an editor.

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/B_Slabs
  (generate_b_slabs.py; cm data, +X street face, pivot on the wall centre line at mid-length, UCX hulls,
  colour attribute R soot / G plaster loss / B dust exported linear). Contract: BerlinRuinKit/KIT_CONTRACT.md.
Destination: /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Meshes

Legacy FBX importer (Interchange drops UCX hulls and ignores FbxImportUI), vertex colours replaced from the FBX.
Materials are assigned by SLOT NAME: M_BerlinRuin_Wall -> MI_BerlinRuin_Wall, M_BerlinRuin_Rubble -> MI_BerlinRuin_Rubble
(B8 has only the rubble slot, at index 0). A slot whose instance does not exist yet is left empty and reported.
Only assets inside the Meshes folder are created or saved.
"""
import json
import os

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/B_Slabs/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Meshes'
MATS = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/'
SLOT_TO_INSTANCE = {'M_BerlinRuin_Wall': 'MI_BerlinRuin_Wall', 'M_BerlinRuin_Rubble': 'MI_BerlinRuin_Rubble'}
FALLBACK_MESHES = (
    'SM_BerlinRuin_B1_Facade1St_A', 'SM_BerlinRuin_B1_Facade1St_B', 'SM_BerlinRuin_B2_Facade2St_A',
    'SM_BerlinRuin_B2_Facade2St_B', 'SM_BerlinRuin_B3_Facade4St', 'SM_BerlinRuin_B4_Firewall', 'SM_BerlinRuin_B5_CornerL',
    'SM_BerlinRuin_B6_Stub_A', 'SM_BerlinRuin_B6_Stub_B', 'SM_BerlinRuin_B7_WallCore', 'SM_BerlinRuin_B8_DebrisBed',
)


def mesh_names():
    """The one name table lives in the generator's manifest; the tuple above is only a fallback."""
    path = MODEL + 'b_slabs_manifest.json'
    if os.path.exists(path):
        with open(path) as f:
            return [p['name'] for p in json.load(f)['pieces']]
    return list(FALLBACK_MESHES)


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


def finish_mesh(name):
    mesh = unreal.load_asset(HOME + '/' + name)
    if mesh is None:
        raise RuntimeError('import made nothing for ' + name)
    slots = mesh.get_editor_property('static_materials')
    assigned, missing = [], []
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        inst_name = SLOT_TO_INSTANCE.get(slot_name)
        inst = unreal.load_asset(MATS + inst_name) if inst_name and unreal.EditorAssetLibrary.does_asset_exist(
            MATS + inst_name) else None
        if inst is not None:
            mesh.set_material(index, inst)
            assigned.append(inst_name)
        else:
            missing.append(inst_name or slot_name)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    mesh.modify()
    unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    has_vc = None
    try:
        has_vc = bool(sms.has_vertex_colors(mesh))
    except Exception:  # noqa: BLE001  (older API names)
        has_vc = None
    info = {'mesh': mesh.get_path_name(), 'convex': hulls,
            'bounds': [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
            'verts': sms.get_number_verts(mesh, 0),
            'slots': [str(s.get_editor_property('material_slot_name')) for s in slots],
            'materials': [(mesh.get_material(i).get_name() if mesh.get_material(i) else None) for i in range(len(slots))],
            'missing_instances': missing, 'vertex_colors': has_vc,
            'uv_channels': sms.get_num_uv_channels(mesh, 0)}
    unreal.log_warning('BERLIN_RUINKIT_B ' + json.dumps(info))
    return info


def run():
    names = mesh_names()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in names:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    return [finish_mesh(name) for name in names]


if __name__ == '__main__':
    run()
