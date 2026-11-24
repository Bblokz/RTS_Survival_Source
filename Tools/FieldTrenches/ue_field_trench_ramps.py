"""Imports the vehicle ramps of a field trench kit (2026-11-12): <piece>_Ramp.fbx next to each kit piece's FBX, written by
generate_field_trenches.py (German) / generate_sov_field_trenches.py (Soviet).

Each ramp is a static mesh whose UCX_ hulls become its simple collision: convex hulls over the piece's earth bank and bag
walls, with a lid at rim height. ARTSFieldTrench attaches one per piece (FieldTrenchLayout::GetPieceRampMeshPath),
hidden and physics only, so vehicle chassis drive over the trench. Simple collision only (no complex trimesh), no
Nanite, no LODs, no material.

Run inside the editor (execute_python):
    import sys; sys.path.append('D:/UE5Projects/RTS_Survival/Source/Tools/FieldTrenches')
    import ue_field_trench_ramps as r
    r.import_ramps(<Blender Exports dir>, <manifest copy in the project>, <mesh folder>)
"""
import json

import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def _ramp_options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    data = options.static_mesh_import_data
    data.set_editor_property('combine_meshes', True)
    # On: the UCX_ hulls are then imported as the collision (off, the Interchange importer drops collision altogether).
    data.set_editor_property('auto_generate_collision', True)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    return options


def import_ramps(exports_dir, manifest_path, mesh_dir, only=None):
    """Imports every piece's ramp; returns [(name, convex hull count, expected count)]. Raises on a missing ramp."""
    with open(manifest_path) as handle:
        records = json.load(handle)['pieces']
    results = []
    for record in records:
        if only and record['name'] not in only:
            continue
        ramp = record.get('ramp')
        if not ramp:
            raise RuntimeError(record['name'] + ': no ramp in the manifest; regenerate the kit')
        name = ramp['fbx'][:-len('.fbx')]
        task = unreal.AssetImportTask()
        task.filename = f'{exports_dir}/{ramp["fbx"]}'
        task.destination_path = mesh_dir
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        task.options = _ramp_options()
        asset_tools.import_asset_tasks([task])
        mesh = unreal.load_asset(f'{mesh_dir}/{name}')
        if mesh is None:
            raise RuntimeError('Import produced nothing for ' + name)
        body = mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        mesh.set_editor_property('lod_group', 'None')
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(
            mesh, unreal.MeshNaniteSettings(enabled=False), apply_changes=True)
        hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems'))
        saved = eal.save_loaded_asset(mesh, False)
        print('FIELDTRENCH_RAMP_IMPORT', name, 'convex', hulls, 'expected', ramp['hulls'], 'saved', saved)
        if hulls != ramp['hulls']:
            raise RuntimeError(f'{name}: {hulls} convex hulls imported, the manifest has {ramp["hulls"]}')
        results.append((name, hulls, ramp['hulls']))
    return results


BANK_SLOT = 'FieldTrench_Bank'


def disable_bank_collision(mesh):
    """Turns collision off on the bank's section (2026-11-12): the bank is for the eye only. In the piece's complex
    collision it blocked the landscape channel 5 m around every trench, so a trench planned next to another one found
    its ground on the neighbour's bank; weapon and visibility traces keep seeing the walls as before. Vehicles drive
    over the separate ramp. Returns the section index, or -1 when the mesh has no bank slot."""
    slots = [str(s.material_slot_name) for s in mesh.static_materials]
    if BANK_SLOT not in slots:
        return -1
    index = slots.index(BANK_SLOT)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if mesh.get_num_sections(0) != len(slots):
        raise RuntimeError(f'{mesh.get_name()}: {mesh.get_num_sections(0)} sections for {len(slots)} slots')
    subsystem.enable_section_collision(mesh, False, 0, index)
    if subsystem.is_section_collision_enabled(mesh, 0, index):
        raise RuntimeError(mesh.get_name() + ': bank section collision still on')
    return index
