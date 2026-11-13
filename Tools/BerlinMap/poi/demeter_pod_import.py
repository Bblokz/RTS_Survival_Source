"""Imports the Demeter pod POI for Ger_Berlin (headless -run=pythonscript, or run() in an editor). Safe to rerun.

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinDemeterPod/ (generate_demeter_pod.py,
validate_demeter_pod.py, make_demeter_label.py, report). Centimetre FBX, +X = flight direction (nose end), pivot at the
ground-level footprint centre, 3 UCX hulls (rear hull, nose section, canister heap). UE mirrors Y on import.

Destination: /Game/RTS_Survival/Blueprints/Environment/Berlin/DemeterPod
- SM_Berlin_DemeterPod (replaced on rerun) + T_DP_DemeterLabel (label/stencil atlas, sRGB).
- Slot M_DP_RustMetal -> MI_Rusty_Painted_Metal_Sheet_vckjca2_2K (Megascans rust, used as is; UVs tile every 2 m).
- Slot M_DP_Earth     -> MI_DP_BerlinMud: child of MI_Arid_Soil_Ground_wihhfexbw_2K re-textured with the Ger_Berlin
                         landscape mud layer (T_Berlin_BerlinMud_D/_N), so the furrow matches the map ground (5 m tile).
- Slot M_DP_Label     -> MI_DP_DemeterLabel: child of MI_RTS_Destroyed_Metal (MM_RTS_Env) with Diffuse = the atlas.

    import importlib, sys; sys.path.append(r'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi')
    import demeter_pod_import as d; importlib.reload(d); d.run()
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinDemeterPod/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/Berlin/DemeterPod'
MESH = 'SM_Berlin_DemeterPod'
LABEL_TEX = 'T_DP_DemeterLabel'
RUST_MI = '/Game/Megascans/Surfaces/Rusty_Painted_Metal_Sheet_vckjca2/MI_Rusty_Painted_Metal_Sheet_vckjca2_2K'
SOIL_MI = '/Game/Megascans/Surfaces/Arid_Soil_Ground_wihhfexbw/MI_Arid_Soil_Ground_wihhfexbw_2K'
BERLIN_TEX = '/Game/RTS_Survival/Materials/ground/Landscape/Berlin/Textures/'
WRECK_METAL_MI = '/Game/RTS_Survival/Materials/Destroyed/MI_RTS_Destroyed_Metal'
EDIT = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def save(obj):
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + obj.get_path_name())


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError('missing asset ' + path)
    return asset


def import_label():
    task = unreal.AssetImportTask()
    task.filename = MODEL + LABEL_TEX + '.png'
    task.destination_path = HOME
    task.destination_name = LABEL_TEX
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = load(f'{HOME}/{LABEL_TEX}')
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    save(tex)
    return tex


def import_mesh():
    """Legacy FBX path: Interchange drops UCX hulls and ignores FbxImportUI."""
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
        TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = load(f'{HOME}/{MESH}')
    mesh.set_editor_property('light_map_coordinate_index', 1)
    return mesh


def instance(name, parent, textures):
    path = f'{HOME}/{name}'
    mi = unreal.load_asset(path)
    if mi is None:
        mi = TOOLS.create_asset(name, HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    EDIT.set_material_instance_parent(mi, parent)
    for key, tex in textures.items():
        EDIT.set_material_instance_texture_parameter_value(mi, key, tex)   # its bool return is unreliable: read back
        got = EDIT.get_material_instance_texture_parameter_value(mi, key)
        if got is None or got.get_path_name() != tex.get_path_name():
            raise RuntimeError(f'{name}: texture parameter {key} not set (reads {got})')
    EDIT.update_material_instance(mi)
    save(mi)
    return mi


def make_materials(label_tex):
    mud = instance('MI_DP_BerlinMud', load(SOIL_MI), {'Albedo': load(BERLIN_TEX + 'T_Berlin_BerlinMud_D'),
                                                       'Normal': load(BERLIN_TEX + 'T_Berlin_BerlinMud_N')})
    label = instance('MI_DP_DemeterLabel', load(WRECK_METAL_MI), {'Diffuse': label_tex})
    return {'M_DP_RustMetal': load(RUST_MI), 'M_DP_Earth': mud, 'M_DP_Label': label}


def run():
    label_tex = import_label()
    mats = make_materials(label_tex)
    mesh = import_mesh()
    slots = mesh.get_editor_property('static_materials')
    for index, slot in enumerate(slots):
        name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in mats if name.startswith(k)), None)
        if key is None:
            raise RuntimeError('unexpected material slot ' + name)
        mesh.set_material(index, mats[key])
    save(mesh)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    uvs = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0)
    info = {'mesh': mesh.get_path_name(), 'triangles': mesh.get_num_triangles(0), 'uv_channels': uvs, 'convex': hulls,
            'bounds_cm': [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
            'slots': {str(s.get_editor_property('material_slot_name')): mesh.get_material(i).get_path_name()
                      for i, s in enumerate(slots)},
            'label_texture': label_tex.get_path_name()}
    unreal.log_warning('BERLIN_DEMETERPOD ' + str(info))
    return info


if __name__ == '__main__':
    run()
