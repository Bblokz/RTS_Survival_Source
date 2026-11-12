# Unreal (run through UECP execute_uecp_script so `uecp` is available). PROTOTYPE, delete with the folder.
# Shared by both deploy prototypes: copies the HQ slot materials into the prototype's own folder, adds the
# prototype's deploy material function to each copy's World Position Offset and assigns the copies to the
# prototype mesh. Originals are never edited (several are shared with the tanks).
# Expects: PROTO_MESH, MAT_FOLDER, SUFFIX, MF_PATH. Optional KEEP_ORIGINAL (material names used only by the truck).
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SOURCE_MESH = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/BuildingMeshes/HQ_BuildingMesh'
KEEP_ORIGINAL = globals().get('KEEP_ORIGINAL', ('M_PantherG_Wheels_Winter', 'MI_T26_Track'))


def node_count(material):
    return MEL.get_num_material_expressions(material)


def duplicate(asset, name):
    path = MAT_FOLDER + '/' + name
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path), False
    return TOOLS.duplicate_asset(name, MAT_FOLDER, asset), True


def chain_copy(original, mf):
    """The Megascans chain MI sits on a Material Attributes master; a plain copy of its look is simpler to wire."""
    name = 'M_Chain' + SUFFIX
    path = MAT_FOLDER + '/' + name
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    textures = {str(p.parameter_info.name): p.parameter_value for p in original.get_editor_property('texture_parameter_values')}
    mat = TOOLS.create_asset(name, MAT_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    albedo = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -200)
    albedo.set_editor_property('texture', textures['albedo'])
    normal = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 100)
    normal.set_editor_property('texture', textures['normal'])
    normal.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 400)
    rough.set_editor_property('texture', textures['DR'])
    rough.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    metal = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 0)
    metal.set_editor_property('r', 0.8)
    P = unreal.MaterialProperty
    MEL.connect_material_property(albedo, 'RGB', P.MP_BASE_COLOR)
    MEL.connect_material_property(normal, 'RGB', P.MP_NORMAL)
    MEL.connect_material_property(rough, 'G', P.MP_ROUGHNESS)
    MEL.connect_material_property(metal, '', P.MP_METALLIC)
    add_deploy_wpo(mat, mf)
    MEL.recompile_material(mat)
    return mat


def add_deploy_wpo(material, mf):
    call = MEL.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, -900, 900)
    call.set_material_function(mf)  # the setter builds the call's outputs; set_editor_property leaves none
    index = node_count(material) - 1
    result = uecp.material.connect_material_nodes(material_path=material.get_path_name(), from_node=index,
                                                  from_output='', to_node=-1, to_input='WorldPositionOffset')
    print('   wpo wired', material.get_name(), index, str(result)[:200])


def deploy_copy(original, mf):
    """Returns the prototype copy of an HQ slot material (MI copies get a copied parent)."""
    if original.get_name() == 'MI_Wall_Mounted_Chain_uiziegbdw_1K':
        return chain_copy(original, mf)
    if isinstance(original, unreal.MaterialInstanceConstant):
        parent = original.get_editor_property('parent')
        parent_copy = deploy_copy(parent, mf)
        mi, created = duplicate(original, original.get_name() + SUFFIX)
        if created:
            MEL.set_material_instance_parent(mi, parent_copy)
        return mi
    return rebuilt_copy(original, mf)


exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/DeployAnimProto/ue_special_copies.py').read(), globals())
# Duplicates of these compiled the added WPO (vertex shader 289 -> 511); duplicates of the others did not.
DUPLICATE_OK = ('M_Pump_station', 'M_BarrelCollection1', 'M_PanzerIII_DesertCamo', 'M_PantherG_Attached_Tracks')
# Material-level settings carried over to the rebuilt copy (the graph comes over through export/import).
COPIED_SETTINGS = ('blend_mode', 'two_sided', 'opacity_mask_clip_value', 'shading_model', 'tangent_space_normal',
                   'dithered_lod_transition', 'used_with_static_lighting')


def rebuilt_copy(original, mf):
    """A NEW material with the original's graph imported, plus the deploy WPO.
    A duplicate_asset copy of some of the HQ masters (scaffold, props atlas, Panther II...) never compiled the
    added WPO (vertex shader stayed at the original's instruction count); a fresh material does."""
    name = original.get_name() + SUFFIX
    path = MAT_FOLDER + '/' + name
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    if original.get_name() in DUPLICATE_OK:
        mat = TOOLS.duplicate_asset(name, MAT_FOLDER, original)
        for usage in ('used_with_skeletal_mesh', 'used_with_niagara_mesh_particles'):  # max 4 UV sets there
            mat.set_editor_property(usage, False)
        route = 'duplicate'
    else:
        mat = TOOLS.create_asset(name, MAT_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        for setting in COPIED_SETTINGS:
            try:
                mat.set_editor_property(setting, original.get_editor_property(setting))
            except Exception as exc:
                print('   setting not copied', setting, exc)
        if original.get_name() in SPECIAL_BUILDERS:
            SPECIAL_BUILDERS[original.get_name()](original, mat)
            route = 'hand-wired'
        else:
            # export/import keeps plain node-to-node wires (no material-function inputs in these graphs)
            graph = uecp.material.export_material_graph(material_path=original.get_path_name())
            result = uecp.material.import_material_graph(material_path=mat.get_path_name(), nodes=graph['nodes'],
                                                         connections=graph['connections'],
                                                         material_outputs=graph['material_outputs'], mode='overwrite')
            route = 'imported %s' % {k: result.get(k) for k in ('nodes_added', 'nodes_failed', 'outputs_failed')}
    print('   built', name, route)
    add_deploy_wpo(mat, mf)
    MEL.recompile_material(mat)
    print('   vs instructions', name, MEL.get_statistics(mat).num_vertex_shader_instructions,
          'original', MEL.get_statistics(original).num_vertex_shader_instructions)
    return mat


mf = unreal.load_asset(MF_PATH)
source = unreal.load_asset(SOURCE_MESH)
mesh = unreal.load_asset(PROTO_MESH)
by_slot = {str(s.get_editor_property('material_slot_name')).rsplit('_0', 1)[0]: s.material_interface
           for s in source.get_editor_property('static_materials')}
for i, slot in enumerate(mesh.get_editor_property('static_materials')):
    name = str(slot.get_editor_property('material_slot_name'))
    original = None
    for key, mat in by_slot.items():
        if mat and (mat.get_name() == name or key == name):
            original = mat
    if original is None:
        print('NO SOURCE MATERIAL for slot', name)
        continue
    target = original if original.get_name() in KEEP_ORIGINAL else deploy_copy(original, mf)
    mesh.set_material(i, target)
    print('slot', i, name, '->', target.get_path_name())
for p in EAL.list_assets(MAT_FOLDER, recursive=False):
    EAL.save_asset(str(p).split('.')[0], only_if_is_dirty=False)
EAL.save_asset(PROTO_MESH, only_if_is_dirty=False)
