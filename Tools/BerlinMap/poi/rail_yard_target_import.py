"""Imports the Ger_Berlin rail yard sabotage target (headless: -run=pythonscript with a wrapper that calls run(), or run() in
an editor). Two meshes so the mission can make each its own sabotage objective:

- SM_Berlin_RailRamp      end + side loading ramp (deck 125 cm, 1:6 slope), buffer stop, guard hut, props. 6 UCX hulls.
- SM_Berlin_SwitchTower   two-storey brick Stellwerk with lever cabin, steel stair, rodding, phone pole, Soviet mast. 2 UCX.

Source (generator, validator, previews, report):
    D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRailYardTarget/
Destination: /Game/RTS_Survival/Blueprints/Environment/Berlin/RailYardTarget (assets replaced on rerun).

Unreal local frame (UE mirrors Blender Y on import):
- Ramp: track centreline at X = -260 cm running along Y, buffer stop face at Y = +775 cm (+Y = toward the buffer stop),
  side platform face at X = -70 cm, slope runs from Y = -375 (deck top) down to Y = -1125 (ground). Rail centres
  X -352.5 / -167.5 (SM_RTS_Railway_Track_01 gauge). The deck assumes rail top ~24 cm above ground, i.e. the track panel
  sunk so its ballast top meets the ground (track actor z = ground - 35 cm), not the 154 cm embankment the map's
  bounds-based placement gives.
- Tower: its track side (point rodding, cable trough) faces -X; the stair is on +X.

Materials are existing project instances, assigned per slot name (the mesh UVs are built for them):
    M_RY_Brick    MI_Berlin_Ruin_Brick (DBK brick, sooty Berlin ruin variant). UV0 = 3 m per tile and UV3 = DBK theme
                  cells, as DBK's own wall meshes; vertex colour black (no DBK damage/dirt selection).
    M_RY_Concrete MI_BF_SootedFortress_UVTile (UV0 = 4 m per tile = BF_Tiling 4)
    M_RY_Timber   MilitaryAirport MI_Planks_1 (UV0 = 2 m per tile, U along the grain)
    M_RY_Steel    Megascans MI_Rusty_Metal_vmghfhro_4K (UV0 = 2 m per tile)
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRailYardTarget/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RailYardTarget'
MATERIALS = {
    'M_RY_Brick': '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/MI_Berlin_Ruin_Brick',
    'M_RY_Concrete': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/'
                     'MI_BF_SootedFortress_UVTile',
    'M_RY_Timber': '/Game/MilitaryAirport/Materials/Tileable/MI_Planks_1',
    'M_RY_Steel': '/Game/Megascans/Surfaces/Rusty_Metal_vmghfhro/MI_Rusty_Metal_vmghfhro_4K',
}
MESHES = ('SM_Berlin_RailRamp', 'SM_Berlin_SwitchTower')


def import_mesh(name):
    """Legacy FBX: Interchange drops UCX hulls and ignores FbxImportUI (ue_flak88_bunker_import.py)."""
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
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in MATERIALS if slot_name.startswith(k)), None)
        if key is None:
            raise RuntimeError('%s: unexpected material slot %s' % (name, slot_name))
        material = unreal.load_asset(MATERIALS[key])
        if material is None:
            raise RuntimeError('%s: material missing %s' % (name, MATERIALS[key]))
        mesh.set_material(index, material)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    mesh.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False):
        raise RuntimeError('save failed for ' + mesh.get_path_name())
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    uvs = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0)
    sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    colours = sme.has_vertex_colors(mesh) if hasattr(sme, 'has_vertex_colors') else 'unknown'
    info = {'mesh': mesh.get_path_name(), 'tris': mesh.get_num_triangles(0), 'uv_channels': uvs, 'convex': hulls,
            'vertex_colors': colours,
            'bounds': [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
            'slots': [str(s.get_editor_property('material_slot_name')) for s in slots],
            'materials': [mesh.get_material(i).get_name() for i in range(len(slots))]}
    unreal.log_warning('BERLIN_RAILYARD ' + str(info))
    return info


def run():
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in MESHES:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    return {name: finish_mesh(name) for name in MESHES}


if __name__ == '__main__':
    run()
