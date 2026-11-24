# Unreal (editor python). Per building: makes the deploy material instances for the VAT deploy mesh and assigns them.
# Each slot gets MI_<Building>_Deploy_<n> = child of the building mesh's own slot material with the static switch
# NomadicDeploy ON and the building's VAT textures, so the deploy looks exactly like the building.
# Material-attributes masters (Megascans) can't take the switch: those slots get a child of M_NomadicDeploy_MSPlain
# with the slot's albedo / normal textures. Run ue_patch_masters.py on SOURCE_MESH first.
# Globals: BUILDING (label), SOURCE_MESH (building mesh), DEPLOY_MESH (imported VAT mesh), OFFSET_TEX, ROT_TEX.
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SHARED = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/Materials'
MS_PLAIN = SHARED + '/M_NomadicDeploy_MSPlain'
MF_PATH = SHARED + '/MF_NomadicDeploy_VAT'


def root_material(mi):
    cur = mi
    while isinstance(cur, unreal.MaterialInstance):
        cur = cur.get_editor_property('parent')
    return cur


def texture_params(mi):
    out = {}
    cur = mi
    while isinstance(cur, unreal.MaterialInstance):
        for p in cur.get_editor_property('texture_parameter_values'):
            out.setdefault(str(p.parameter_info.name).lower(), p.parameter_value)
        cur = cur.get_editor_property('parent')
    return out


def ensure_ms_plain():
    """Plain lit master for Megascans attribute materials: Albedo / Normal / DR (roughness in G) + deploy WPO."""
    if EAL.does_asset_exist(MS_PLAIN):
        return unreal.load_asset(MS_PLAIN)
    m = TOOLS.create_asset('M_NomadicDeploy_MSPlain', SHARED, unreal.Material, unreal.MaterialFactoryNew())
    P = unreal.MaterialProperty
    specs = (('Albedo', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'RGB', P.MP_BASE_COLOR, '/Engine/EngineResources/DefaultTexture'),
             ('Normal', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 'RGB', P.MP_NORMAL, '/Engine/EngineMaterials/DefaultNormal'),
             # default must itself be a masks texture or the master fails to compile (sampler type mismatch)
             ('DR', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 'G', P.MP_ROUGHNESS,
              '/Game/RTS_Survival/Meshes/Props/Chains/Wall_Mounted_Chain_uiziegbdw/T_WallMountedChain_uiziegbdw_1K_DpR'))
    for k, (name, sampler, pin, prop, default) in enumerate(specs):
        t = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, -200 + 260 * k)
        t.set_editor_property('parameter_name', name)
        t.set_editor_property('texture', unreal.load_asset(default))
        t.set_editor_property('sampler_type', sampler)
        MEL.connect_material_property(t, pin, prop)
    metal = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 600)
    metal.set_editor_property('parameter_name', 'Metallic')
    metal.set_editor_property('default_value', 0.6)
    MEL.connect_material_property(metal, '', P.MP_METALLIC)
    call = MEL.create_material_expression(m, unreal.MaterialExpressionMaterialFunctionCall, -600, 800)
    call.set_material_function(unreal.load_asset(MF_PATH))
    unreal.NomadicDeployEditorLibrary.connect_world_position_offset(call, '')
    unreal.NomadicDeployEditorLibrary.refresh_material(m)
    EAL.save_loaded_asset(m, False)
    return m


STATIC_ONLY_SUFFIX = '_NomadicDeploy'
SKELETAL_USAGES = ('used_with_skeletal_mesh', 'used_with_morph_targets', 'used_with_clothing')


def static_only(material):
    """MF_NomadicDeploy_VAT reads UV4/UV5 (6 vertex texcoords) and the GPU-skin vertex factory takes at most 4. Under a
    master that is also used on skeletal meshes, the cook fails that permutation, drops the deploy MI's shader map and
    Shipping draws the default material (the editor hides this). So switch-mode deploy MIs parent to a static-only
    copy of the slot material's chain: <name>_NomadicDeploy, the master copy with the skeletal usages off."""
    root = root_material(material)
    if not any(root.get_editor_property(usage) for usage in SKELETAL_USAGES):
        return material
    path = material.get_path_name().split('.')[0]
    if path.endswith(STATIC_ONLY_SUFFIX):
        return material
    copy_path = path + STATIC_ONLY_SUFFIX
    if not EAL.does_asset_exist(copy_path):
        EAL.duplicate_asset(path, copy_path)
    copy = unreal.load_asset(copy_path)
    if isinstance(material, unreal.MaterialInstance):
        MEL.set_material_instance_parent(copy, static_only(material.get_editor_property('parent')))
        MEL.update_material_instance(copy)
    else:
        for usage in SKELETAL_USAGES:
            copy.set_editor_property(usage, False)
        MEL.recompile_material(copy)
    EAL.save_loaded_asset(copy, False)
    return copy


def deploy_mi(index, original, folder, offset_tex, rot_tex, frames):
    name = 'MI_%s_Deploy_%02d' % (BUILDING, index)
    path = folder + '/' + name
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    root = root_material(original)
    if root.get_editor_property('use_material_attributes'):
        MEL.set_material_instance_parent(mi, ensure_ms_plain())
        tex = texture_params(original)
        # Megascans name them albedo / normal / dr; merged-actor (FlattenMaterial) slots use BaseColorTexture /
        # NormalTexture / RoughnessTexture or PackedTexture
        for dst, keys in (('Albedo', ('albedo', 'basecolortexture', 'basecolor', 'diffuse')),
                          ('Normal', ('normal', 'normaltexture')),
                          ('DR', ('dr', 'roughnesstexture', 'packedtexture'))):
            found = next((tex[k] for k in keys if tex.get(k)), None)
            if found:
                MEL.set_material_instance_texture_parameter_value(mi, dst, found)
        kind = 'ms-plain'
    else:
        MEL.set_material_instance_parent(mi, static_only(original))
        MEL.set_material_instance_static_switch_parameter_value(mi, 'NomadicDeploy', True)
        kind = 'switch'
    MEL.set_material_instance_texture_parameter_value(mi, 'NomadicDeployVatOffset', offset_tex)
    MEL.set_material_instance_texture_parameter_value(mi, 'NomadicDeployVatRot', rot_tex)
    MEL.set_material_instance_scalar_parameter_value(mi, 'NomadicDeployVatFrames', float(frames))
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    return mi, kind


source = unreal.load_asset(SOURCE_MESH)
deploy = unreal.load_asset(DEPLOY_MESH)
offset_tex = unreal.load_asset(OFFSET_TEX)
rot_tex = unreal.load_asset(ROT_TEX)
frames = offset_tex.blueprint_get_size_y()
folder = DEPLOY_MESH.rsplit('/', 1)[0]
src_slots = source.get_editor_property('static_materials')
dst_slots = deploy.get_editor_property('static_materials')
if len(src_slots) != len(dst_slots):
    raise RuntimeError('slot count differs: source %d deploy %d' % (len(src_slots), len(dst_slots)))
for i, slot in enumerate(src_slots):
    original = slot.material_interface
    if original is None:
        print('slot', i, 'has no material, left empty')
        continue
    mi, kind = deploy_mi(i, original, folder, offset_tex, rot_tex, frames)
    deploy.set_material(i, mi)
    print('slot', i, str(slot.material_slot_name), '->', mi.get_name(), kind, 'parent', original.get_name())
print('saved deploy mesh', EAL.save_loaded_asset(deploy, False), 'frames', frames)
