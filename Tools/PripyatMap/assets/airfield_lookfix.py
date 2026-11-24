"""Airfield look fixes from the lead's first in-game review (2026-11-14). Kit-owned assets only.

1. Ridge platforms: T_Prypyat_RidgeEarth_base_color re-imported from T_Prypyat_RidgeEarth_Dry.png (dry straw grass,
   sRGB ~150/135/95, over bare brown earth; pre-divided by the material's 0.72/0.57/0.38 tint) and
   T_Prypyat_RidgeConcrete_base_color from T_Prypyat_RidgeConcrete_Weathered.png (dark weathered grey). Both masters
   and the concrete _WallHoles copy sample these textures, so intact and holed looks stay identical.
2. Runway (SM_SovietAirfield_Prypyat, kit copy): slot 8 lens -> MI_Prypyat_RunwayLightsOff (Emissive_Strength 0);
   M_Prypyat_Runway gets a second Custom node (ruin pass) after the kit's one: ConcreteDarken, slab-joint grass/dirt
   (SlabSize, JointWidth, JointGrass), crack grass (CrackGrass, surface.g), margin grass (MarginGrass, markings.a);
   MI_Prypyat_Runway: PaintStrength 0.35, Weathering 1.4, RepairStrength 0.9, Wetness 0.5 + the new parameters.
"""
import unreal

_A = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield'
_MEL = unreal.MaterialEditingLibrary
_EAL = unreal.EditorAssetLibrary
_AT = unreal.AssetToolsHelpers.get_asset_tools()
_SRC = 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/'
_TAG, _OWN = 'PrypyatAirfieldOwner', 'PrypyatAirfield_20261113'


def _reimport(png, name):
    task = unreal.AssetImportTask()
    task.filename, task.destination_path, task.destination_name = _SRC + png, _A + '/Textures', name
    task.automated, task.replace_existing, task.save = True, True, False
    _AT.import_asset_tasks([task])
    tex = unreal.load_asset(_A + '/Textures/' + name)
    tex.set_editor_property('srgb', True)
    _EAL.set_metadata_tag(tex, _TAG, _OWN)
    _EAL.save_loaded_asset(tex, False)
    print('texture', name, tex.blueprint_get_size_x())


_reimport('T_Prypyat_RidgeEarth_Dry.png', 'T_Prypyat_RidgeEarth_base_color')
_reimport('T_Prypyat_RidgeConcrete_Weathered.png', 'T_Prypyat_RidgeConcrete_base_color')
for _m in ('M_Prypyat_RidgeEarth', 'M_Prypyat_RidgeConcrete', 'M_Prypyat_RidgeConcrete_WallHoles'):
    _mat = unreal.load_asset(_A + '/Materials/' + _m)
    _MEL.recompile_material(_mat)

# ---------------------------------------------------------------- runway lights off
_lens = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz7_Flak/Materials/MI_Sdkfz7Flak_SearchlightLens'
_off_path = _A + '/Materials/MI_Prypyat_RunwayLightsOff'
_off = unreal.load_asset(_off_path) if _EAL.does_asset_exist(_off_path) else _AT.create_asset(
    'MI_Prypyat_RunwayLightsOff', _A + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
_MEL.set_material_instance_parent(_off, unreal.load_asset(_lens))
_MEL.set_material_instance_scalar_parameter_value(_off, 'Emissive_Strength', 0.0)
_MEL.set_material_instance_vector_parameter_value(_off, 'Emissive_Color', unreal.LinearColor(0, 0, 0, 1))
_MEL.update_material_instance(_off)
_EAL.set_metadata_tag(_off, _TAG, _OWN)
_EAL.save_loaded_asset(_off, False)
_mesh = unreal.load_asset(_A + '/Meshes/SM_SovietAirfield_Prypyat')
_mesh.set_material(8, _off)
_EAL.save_loaded_asset(_mesh, False)

# ---------------------------------------------------------------- runway ruin pass
_RUIN_HLSL = r'''
float2 uv = float2((LocalP.x+10800.0)/21600.0,(LocalP.y+7700.0)/14000.0);
float4 marking=Texture2DSample(Marking,MarkingSampler,uv);
float4 surface=Texture2DSample(Surface,SurfaceSampler,uv);
float3 color = Prev.rgb*ConcreteDarken;
float2 cell = abs(frac(LocalP.xy/SlabSize)-0.5)*SlabSize;
float2 toJoint = SlabSize*0.5-cell;
float joint = 1.0-smoothstep(JointWidth*0.5, JointWidth*1.6, min(toJoint.x,toJoint.y));
float growth = saturate(joint*JointGrass + surface.g*CrackGrass + marking.a*MarginGrass);
float3 earth = float3(.055,.04,.025);
float3 straw = float3(.24,.19,.09);
float3 g = lerp(earth, straw, saturate(surface.a*1.6));
color = lerp(color, g, growth);
return float4(color, lerp(Prev.a, 0.95, growth));
'''
_NEW = {'ConcreteDarken': 0.62, 'SlabSize': 500.0, 'JointWidth': 14.0, 'JointGrass': 0.85, 'CrackGrass': 0.6,
        'MarginGrass': 0.7}
_MI_VALUES = {'PaintStrength': 0.35, 'Weathering': 1.4, 'RepairStrength': 0.9, 'Wetness': 0.5}
_mat = unreal.load_asset(_A + '/Materials/M_Prypyat_Runway')
if 'ConcreteDarken' not in [str(n) for n in _MEL.get_scalar_parameter_names(_mat)]:
    _color_mask = _MEL.get_material_property_input_node(_mat, unreal.MaterialProperty.MP_BASE_COLOR)
    _first = [x for x in _MEL.get_inputs_for_material_expression(_mat, _color_mask) if x][0]
    if not isinstance(_first, unreal.MaterialExpressionCustom):
        raise RuntimeError('unexpected runway graph: base colour mask is fed by ' + _first.get_class().get_name())
    _world = _MEL.create_material_expression(_mat, unreal.MaterialExpressionWorldPosition, 900, 1200)
    _local = _MEL.create_material_expression(_mat, unreal.MaterialExpressionTransformPosition, 1100, 1200)
    _local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    _local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    _ok = [_MEL.connect_material_expressions(_world, '', _local, '')]
    _ins = [('Prev', _first, ''), ('LocalP', _local, '')]
    for _k, (_pn, _tn) in enumerate((('PrypyatMarkingMask', 'T_Prypyat_Runway_Markings'),
                                     ('PrypyatSurfaceMask', 'T_Prypyat_Runway_Surface'))):
        _t = _MEL.create_material_expression(_mat, unreal.MaterialExpressionTextureObjectParameter, 1100, 1400 + _k * 180)
        _t.set_editor_property('parameter_name', _pn)
        _t.set_editor_property('texture', unreal.load_asset(_A + '/Materials/' + _tn))
        _t.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        _ins.append((('Marking', 'Surface')[_k], _t, ''))
    for _k, (_n, _v) in enumerate(_NEW.items()):
        _p = _MEL.create_material_expression(_mat, unreal.MaterialExpressionScalarParameter, 1100, 1800 + _k * 120)
        _p.set_editor_property('parameter_name', _n)
        _p.set_editor_property('default_value', _v)
        _p.set_editor_property('group', 'Prypyat Ruin')
        _ins.append((_n, _p, ''))
    _custom = _MEL.create_material_expression(_mat, unreal.MaterialExpressionCustom, 1450, 900)
    _custom.set_editor_property('code', _RUIN_HLSL)
    _custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    _custom.set_editor_property('description', 'Prypyat ruin pass: darken, slab-joint/crack/margin grass and dirt')
    _pins = []
    for _n, _, _ in _ins:
        _ci = unreal.CustomInput()
        _ci.set_editor_property('input_name', _n)
        _pins.append(_ci)
    _custom.set_editor_property('inputs', _pins)
    for _n, _node, _out in _ins:
        _ok.append(_MEL.connect_material_expressions(_node, _out, _custom, _n))
    _cm = _MEL.create_material_expression(_mat, unreal.MaterialExpressionComponentMask, 1700, 850)
    for _c, _on in zip('rgba', (True, True, True, False)):
        _cm.set_editor_property(_c, _on)
    _rm = _MEL.create_material_expression(_mat, unreal.MaterialExpressionComponentMask, 1700, 1000)
    for _c, _on in zip('rgba', (False, False, False, True)):
        _rm.set_editor_property(_c, _on)
    _ok += [_MEL.connect_material_expressions(_custom, '', _cm, ''), _MEL.connect_material_expressions(_custom, '', _rm, ''),
            _MEL.connect_material_property(_cm, '', unreal.MaterialProperty.MP_BASE_COLOR),
            _MEL.connect_material_property(_rm, '', unreal.MaterialProperty.MP_ROUGHNESS)]
    if not all(_ok):
        raise RuntimeError('ruin pass links failed: %s' % _ok)
    _MEL.recompile_material(_mat)
    _EAL.set_metadata_tag(_mat, 'PrypyatRunwayVersion', '1')
    _EAL.save_loaded_asset(_mat, False)
    print('ruin pass added')
_mi = unreal.load_asset(_A + '/Materials/MI_Prypyat_Runway')
for _n, _v in list(_MI_VALUES.items()) + list(_NEW.items()):
    _MEL.set_material_instance_scalar_parameter_value(_mi, _n, _v)
_MEL.update_material_instance(_mi)
_EAL.save_loaded_asset(_mi, False)
_st = _MEL.get_statistics(_mat)
print('runway PS', _st.num_pixel_shader_instructions, 'samplers', _st.num_samplers,
      {str(n): _MEL.get_material_instance_scalar_parameter_value(_mi, n) for n in _MEL.get_scalar_parameter_names(_mi)})
