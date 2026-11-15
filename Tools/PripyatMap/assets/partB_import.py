"""Part B -> /Game/RTS_Survival/Blueprints/Environment/Prypyat/Crossings and .../RadioMast (static meshes).

Makes M_Prypyat_KitSteel (the airfield's rusty crane-metal texture x Tint 0.5: blackened steel) and imports the 16
meshes from partB_report.json with slots Wood = MI_Polesie_FarmAtlas, Steel = M_Prypyat_KitSteel,
Algae = MI_Phytophage_BlackPlum_Solid. exec in the server namespace.
"""
import json

import unreal

_R = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/'
_MEL = unreal.MaterialEditingLibrary
_EAL = unreal.EditorAssetLibrary
_AT = unreal.AssetToolsHelpers.get_asset_tools()
_STEEL = _R + 'Crossings/Materials/M_Prypyat_KitSteel'


def _steel():
    if _EAL.does_asset_exist(_STEEL):
        mat = unreal.load_asset(_STEEL)
        for _ in range(30):
            if _MEL.get_num_material_expressions(mat) == 0:
                break
            _MEL.delete_all_material_expressions(mat)
    else:
        mat = _AT.create_asset('M_Prypyat_KitSteel', _R + 'Crossings/Materials', unreal.Material, unreal.MaterialFactoryNew())
    tex = _MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, 0)
    tex.set_editor_property('texture', unreal.load_asset(_R + 'Airfield/Textures/T_Prypyat_PropMetal_base_color'))
    tint = _MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, -250)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.5, 0.48, 0.46, 1.0))
    mul = _MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, -100)
    rough = _MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, 150)
    rough.set_editor_property('parameter_name', 'Roughness')
    rough.set_editor_property('default_value', 0.72)
    metal = _MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, 300)
    metal.set_editor_property('parameter_name', 'Metallic')
    metal.set_editor_property('default_value', 0.45)
    ok = [_MEL.connect_material_expressions(tex, 'RGB', mul, 'A'), _MEL.connect_material_expressions(tint, '', mul, 'B'),
          _MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR),
          _MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS),
          _MEL.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)]
    if not all(ok):
        raise RuntimeError('steel links %s' % ok)
    _MEL.recompile_material(mat)
    _EAL.save_loaded_asset(mat, False)
    return mat


_steel()
_PB = json.load(open('D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/partB_report.json'))
_SLOTMAT = {'M_Prypyat_KitWood': _R + 'Village/Materials/MI_Polesie_FarmAtlas', 'M_Prypyat_KitSteel': _STEEL,
            'M_Prypyat_KitAlgae': _R + 'Phytophage/MI_Phytophage_BlackPlum_Solid'}
PRY_STATIC = []
for _name, _info in _PB.items():
    if globals().get('PRY_PARTB_ONLY') and not _name.startswith(PRY_PARTB_ONLY):
        continue
    _folder = _R + ('RadioMast' if 'RadioMast' in _name else 'Crossings')
    PRY_STATIC.append(dict(fbx=_info['fbx'], dest_folder=_folder, name=_name,
                           slot_map=_SLOTMAT,
                           collision='ucx' if _info['hulls'] else 'none', lightmap_index=1))
exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets/static_import.py').read())
