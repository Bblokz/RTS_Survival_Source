"""Cliff stone decals: Unreal side (exec'd in the throwaway editor via fol_cmd.py). Re-runnable.

Imports the 8 baked texture sets from the Blender CliffStones folder and makes one instance of the existing
M_OreCliffDecal master per decal. The master itself is never rebuilt (ue_build_decals.py would recreate it).

/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/CliffStones/
  Textures/T_CliffStoneDecal_<Name>_BCA / _N / _RMAO
  MI_CliffStoneDecal_<Name>
Decal actors: decal_apply.py.
"""
import json
import os

import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreCliffKit/Decals/CliffStones/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/CliffStones'
MASTER = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/Materials/M_OreCliffDecal'
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TC = unreal.TextureCompressionSettings


def import_texture(fname, kind):
    name = os.path.splitext(fname)[0]
    task = unreal.AssetImportTask()
    task.filename = SRC + 'Textures/' + fname
    task.destination_path = HOME + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = '%s/Textures/%s' % (HOME, name)
    tex = unreal.load_asset(path)
    if tex is None:
        raise RuntimeError('import failed ' + fname)
    if kind == 'BCA':
        tex.set_editor_property('srgb', True)
        tex.set_editor_property('compression_settings', TC.TC_BC7)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    elif kind == 'N':
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('compression_settings', TC.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel', False)          # authored DirectX, as Unreal wants
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    else:
        tex.set_editor_property('srgb', False)
        tex.set_editor_property('compression_settings', TC.TC_MASKS)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    EAL.save_asset(path, only_if_is_dirty=False)
    return tex


def build():
    doc = json.load(open(SRC + 'decals.json'))
    master = unreal.load_asset(MASTER)
    out = []
    for d in doc['decals']:
        tex = {k: import_texture(f, k) for k, f in d['textures'].items()}
        name = 'MI_CliffStoneDecal_' + d['name']
        path = HOME + '/' + name
        if EAL.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        MEL.set_material_instance_texture_parameter_value(mi, 'BaseColour_Opacity', tex['BCA'])
        MEL.set_material_instance_texture_parameter_value(mi, 'Normal', tex['N'])
        MEL.set_material_instance_texture_parameter_value(mi, 'RMAO', tex['RMAO'])
        # colour, roughness and AO are baked (bake report); the master's lerps stay neutral
        for k, v in (('OpacityLerpA', 0.0), ('OpacityLerpB', float(d.get('opacity_start', 1.0))),
                     ('BaseColour_Brightness', 1.0), ('BaseColour_OpacityScale', 1.0), ('RoughnessLerpA', 0.0),
                     ('RoughnessLerpB', 1.0), ('Normal_Intensity', 1.0), ('Extra Fake AO Strength', 0.0),
                     # fade on steep faces: a down decal never smears the stones up a cliff face or bank
                     ('AngleFadeLo', 0.45), ('AngleFadeHi', 0.75), ('AngleFadeAmount', 1.0), ('DepthFade', 0.4)):       # r4: fade towards the box ends (units)
            MEL.set_material_instance_scalar_parameter_value(mi, k, v)
        MEL.set_material_instance_vector_parameter_value(mi, 'BaseColour_Tint', unreal.LinearColor(1, 1, 1, 1))
        MEL.update_material_instance(mi)
        EAL.save_asset(path, only_if_is_dirty=False)
        out.append(path)
    print('stone decal MIs', len(out))
    return out
