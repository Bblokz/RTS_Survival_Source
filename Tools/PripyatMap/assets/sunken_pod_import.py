"""Imports the sunken Project Demeter pod for Ger_PripyatMe410. Safe to rerun.

    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PripyatSunkenPod/export (README.md there). Centimetre FBX,
+X = nose, pivot at the waterline centre (place at z = 0), 4 UCX hulls, LOD1 in its own FBX.

Destination: /Game/RTS_Survival/Blueprints/Environment/Prypyat/SunkenPod
- SM_Pripyat_SunkenDemeterPod (2 LODs), T_SDP_Hull_D/_N/_ORM/_Mask.
- Slot Hull       -> MI_SDP_Hull, instance of the new M_SDP_Hull (base colour, normal, AO/roughness/metallic).
- Slot Phytophage -> Prypyat/Phytophage/Trees/MI_PhytoTree (unchanged).
- Slot Label      -> Berlin/DemeterPod/MI_DP_DemeterLabel (unchanged).
"""
import unreal

SDP_SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PripyatSunkenPod/export/'
SDP_HOME = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/SunkenPod'
SDP_MESH = 'SM_Pripyat_SunkenDemeterPod'
SDP_PHYTO = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/Trees/MI_PhytoTree'
SDP_LABEL = '/Game/RTS_Survival/Blueprints/Environment/Berlin/DemeterPod/MI_DP_DemeterLabel'
SDP_LOD_SCREEN = [1.0, 0.3]
_EDIT = unreal.MaterialEditingLibrary
_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
_EAL = unreal.EditorAssetLibrary
_SUB = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def _log(*parts):
    unreal.log_warning('SUNKEN_POD ' + ' '.join(str(p) for p in parts))


def _texture(name, kind):
    task = unreal.AssetImportTask()
    task.filename = SDP_SRC + name + '.png'
    task.destination_path = SDP_HOME
    task.destination_name = name
    task.automated, task.save, task.replace_existing = True, False, True
    _TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(SDP_HOME + '/' + name)
    if tex is None:
        raise RuntimeError('texture import failed ' + name)
    compression = {'color': unreal.TextureCompressionSettings.TC_DEFAULT, 'normal': unreal.TextureCompressionSettings.TC_NORMALMAP,
                   'mask': unreal.TextureCompressionSettings.TC_MASKS}[kind]
    tex.set_editor_property('srgb', kind == 'color')
    tex.set_editor_property('compression_settings', compression)
    if kind == 'normal':
        tex.set_editor_property('flip_green_channel', False)   # baked DirectX
    _EAL.save_loaded_asset(tex, False)
    return tex


def _master(tex_d, tex_n, tex_orm):
    path = SDP_HOME + '/M_SDP_Hull'
    if _EAL.does_asset_exist(path):
        _EAL.delete_asset(path)
    mat = _TOOLS.create_asset('M_SDP_Hull', SDP_HOME, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('two_sided', True)   # the sprung lid and the fins are seen from both sides

    def sample(name, tex, sampler, y):
        node = _EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -600, y)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('texture', tex)
        node.set_editor_property('sampler_type', sampler)
        return node

    base = sample('BaseColor', tex_d, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -300)
    normal = sample('Normal', tex_n, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 0)
    orm = sample('ORM', tex_orm, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 300)
    tint = _EDIT.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, -520)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mul = _EDIT.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, -400)
    _EDIT.connect_material_expressions(base, 'RGB', mul, 'A')
    _EDIT.connect_material_expressions(tint, '', mul, 'B')
    rough_scale = _EDIT.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 560)
    rough_scale.set_editor_property('parameter_name', 'RoughnessScale')
    rough_scale.set_editor_property('default_value', 1.0)
    rough = _EDIT.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 420)
    _EDIT.connect_material_expressions(orm, 'G', rough, 'A')
    _EDIT.connect_material_expressions(rough_scale, '', rough, 'B')
    links = [(mul, '', unreal.MaterialProperty.MP_BASE_COLOR), (normal, 'RGB', unreal.MaterialProperty.MP_NORMAL),
             (orm, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), (rough, '', unreal.MaterialProperty.MP_ROUGHNESS),
             (orm, 'B', unreal.MaterialProperty.MP_METALLIC)]
    for node, pin, prop in links:
        if not _EDIT.connect_material_property(node, pin, prop):
            raise RuntimeError('M_SDP_Hull: could not connect %s' % prop)
    _EDIT.recompile_material(mat)
    _EAL.save_loaded_asset(mat, False)
    return mat


def _instance(master):
    path = SDP_HOME + '/MI_SDP_Hull'
    mi = unreal.load_asset(path) if _EAL.does_asset_exist(path) else _TOOLS.create_asset(
        'MI_SDP_Hull', SDP_HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    _EDIT.set_material_instance_parent(mi, master)
    _EDIT.update_material_instance(mi)
    _EAL.save_loaded_asset(mi, False)
    return mi


def _fbx_ui():
    ui = unreal.FbxImportUI()
    for key, value in dict(import_mesh=True, import_as_skeletal=False, import_materials=False, import_textures=False,
                           import_animations=False).items():
        ui.set_editor_property(key, value)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    for key, value in dict(combine_meshes=True, auto_generate_collision=False, one_convex_hull_per_ucx=True,
                           generate_lightmap_u_vs=False, build_nanite=False, remove_degenerates=True).items():
        data.set_editor_property(key, value)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    return ui


def _mesh(materials):
    path = SDP_HOME + '/' + SDP_MESH
    if _EAL.does_asset_exist(path):
        _EAL.delete_asset(path)   # a re-import into the old asset keeps its hulls and slot names
    task = unreal.AssetImportTask()
    task.filename = SDP_SRC + SDP_MESH + '_LOD0.FBX'
    task.destination_path = SDP_HOME
    task.destination_name = SDP_MESH
    task.automated, task.save, task.replace_existing, task.options = True, False, True, _fbx_ui()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        _TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError('mesh import failed')
    got = _SUB.import_lod(mesh, 1, SDP_SRC + SDP_MESH + '_LOD1.FBX')
    if got != 1:
        _log('WARN LOD1 import returned', got)
    _SUB.set_lod_screen_sizes(mesh, SDP_LOD_SCREEN[:mesh.get_num_lods()])
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.enabled = False
    mesh.set_editor_property('nanite_settings', nanite)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    statics = mesh.get_editor_property('static_materials')
    for slot in statics:
        name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in materials if k.lower() in name.lower()), None)
        if key is None:
            raise RuntimeError('unexpected material slot ' + name)
        slot.set_editor_property('material_slot_name', key)
        slot.set_editor_property('material_interface', materials[key])
    mesh.set_editor_property('static_materials', statics)
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        mesh.set_material(index, materials[str(slot.get_editor_property('material_slot_name'))])
    _EAL.save_loaded_asset(mesh, False)
    return mesh


def run():
    tex_d = _texture('T_SDP_Hull_D', 'color')
    tex_n = _texture('T_SDP_Hull_N', 'normal')
    tex_orm = _texture('T_SDP_Hull_ORM', 'mask')
    _texture('T_SDP_Hull_Mask', 'mask')
    hull = _instance(_master(tex_d, tex_n, tex_orm))
    materials = {'Hull': hull, 'Phytophage': unreal.load_asset(SDP_PHYTO), 'Label': unreal.load_asset(SDP_LABEL)}
    for key, mat in materials.items():
        if mat is None:
            raise RuntimeError('missing material for slot ' + key)
    mesh = _mesh(materials)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    slots = {str(s.get_editor_property('material_slot_name')): str(s.get_editor_property('material_interface').get_name()
                                                                   if s.get_editor_property('material_interface') else None)
             for s in mesh.get_editor_property('static_materials')}
    info = {'lods': mesh.get_num_lods(), 'tris': [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
            'convex': hulls, 'uv_channels': _SUB.get_num_uv_channels(mesh, 0), 'slots': slots,
            'bounds_cm': [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)]}
    _log('RESULT', info)
    return info


if __name__ == '__main__':
    run()
