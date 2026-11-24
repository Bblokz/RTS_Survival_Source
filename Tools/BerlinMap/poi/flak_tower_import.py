"""Imports the two flak tower ruins for Ger_Berlin (headless: -run=pythonscript, or run() in an editor).

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/FlakTower/Model (another session's
model, cm, +X front, pivot at the footprint centre, UCX hulls, Gun/LightFlak sockets).
Destination: /Game/RTS_Survival/Blueprints/Environment/Berlin/FlakTower.

This is the plain import the owner asked for on 2026-11-12: Breakthrough concrete with tiling UVs (MI_BF_SootedFortress_UVTile)
and M_BF_Trim. The tower's own curvature and soot maps (bf_fields.py maps) are still to be done by its session.
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/FlakTower/Model/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/Berlin/FlakTower'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/'
MATERIALS = {'M_BF_SootedFortress_Concrete': SHARED + 'MI_BF_SootedFortress_UVTile', 'M_BF_Trim': SHARED + 'M_BF_Trim'}
MESHES = ('SM_BF_FlakTower_Damaged', 'SM_BF_FlakTower_LightDamage')


def import_mesh(name):
    """Legacy FBX: Interchange drops UCX hulls and ignores FbxImportUI."""
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
    for index, slot in enumerate(slots):
        wanted = MATERIALS.get(str(slot.get_editor_property('material_slot_name')))
        if wanted is None:
            wanted = list(MATERIALS.values())[min(index, 1)]
        mesh.set_material(index, unreal.load_asset(wanted))
    mesh.modify()
    unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    info = {'mesh': mesh.get_path_name(), 'convex': hulls,
            'bounds': [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
            'slots': [str(s.get_editor_property('material_slot_name')) for s in slots],
            'materials': [mesh.get_material(i).get_name() for i in range(len(slots))],
            'sockets': [str(s.get_editor_property('socket_name')) for s in mesh.get_sockets_by_tag('')] if hasattr(mesh, 'get_sockets_by_tag') else []}
    unreal.log_warning('BERLIN_FLAKTOWER ' + str(info))
    return info


def run():
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in MESHES:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    return [finish_mesh(name) for name in MESHES]


if __name__ == '__main__':
    run()
