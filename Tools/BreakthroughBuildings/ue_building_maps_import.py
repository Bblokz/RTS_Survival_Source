"""Per-building LightMapUV maps for the Breakthrough concrete (run with the UECP execute_python action), 2026-11-05.

SharedParts/bf_fields.py `maps` bakes, per building, into the building's Blender model folder:
- `T_BF_<Name>_Curv`: the rebar's curvature gate and hoop weights (switch BF_UseCurvatureMap, parameter BF_CurvatureMap);
- `T_BF_<Name>_Fields` with its json (lamp_scale): heat, soot and lamp light (BF_UseBakedFields, BF_FieldsMap, BF_FieldsLampScale);
- `T_BF_<Name>_Camo`: the camo mask's three-axis read (BF_UseCamoMap, BF_CamoMap), where the building is painted.
This imports them next to the mesh and turns them on in the building's own concrete instance, MI_BF_<Name>_Concrete (made by
the building's import or lamps script; run this after those, since they rebuild the instance). It also turns on the shared
surface bake (BF_UseBakedSurface, ProceduralSource/make_surface_bake.py) on MI_BF_SootedFortress_UVTile, which every
building's concrete inherits. Copy `T_BF_<Name>_Fields.json` from the model folder next to this script first: the editor's
Python only opens files inside the project. Rebake when the mesh's LightMapUV, the lamp or heat sources, or the camo mask,
scale or seed change.

    MAPS_TO_RUN = ['HQ']; exec(open(r'<this file>').read())      # default: all
"""
import json

import unreal

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/'
BLEND = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/'
CHILD = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/'
BUNKERS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/MI_BF_SootedFortress_UVTile'
MAPS = {
    'HQ': dict(home=CHILD + 'BreakthroughHQ/Mesh', model=BLEND + 'HQ/Model/', fields=True, camo=True),
    'MechDepot': dict(home=CHILD + 'BreakthroughMechDepot/Mesh', model=BLEND + 'MechanizedDepot/Model/', fields=True),
    'LightFactory': dict(home=CHILD + 'BreakthroughLightFactory/Mesh', model=BLEND + 'LightVehicleFactory/Model/', fields=True),
    'ExperimentalFactory': dict(home=CHILD + 'BreakthroughExperimentalFactory/Mesh', model=BLEND + 'ExperimentalUnitsFactory/Model/', fields=True),
    'OxideWorks': dict(home=CHILD + 'BreakthroughOxideWorks/Mesh', model=BLEND + 'OxideWorks/Model/', fields=True),
    # buildings that used the shared instance get their own, made here (mesh=), for the curvature map alone
    'Flak88Bunker': dict(home=BUNKERS + 'Flak88Bunker/Mesh', model=BLEND + 'Flak88Bunker/Model/', mesh='SM_BF_Flak88Bunker'),
    'Flak88Bunker_Damaged': dict(home=BUNKERS + 'Flak88Bunker/Mesh', model=BLEND + 'Flak88Bunker/Model/', mesh='SM_BF_Flak88Bunker_Damaged'),
    'FlakvierlingBunker': dict(home=BUNKERS + 'FlakvierlingBunker/Mesh', model=BLEND + 'FlakvierlingBunker/Model/', mesh='SM_BF_FlakvierlingBunker'),
    'FlakvierlingBunker_Damaged': dict(home=BUNKERS + 'FlakvierlingBunker/Mesh', model=BLEND + 'FlakvierlingBunker/Model/',
                                       mesh='SM_BF_FlakvierlingBunker_Damaged'),
    'MediumFactory': dict(home=CHILD + 'BreakthroughMediumFactory/Mesh', model=BLEND + 'MediumVehicleFactory/Model/', mesh='SM_BF_MediumFactory'),
}
CONCRETE_SLOT = 'M_BF_SootedFortress_Concrete'
EDIT = unreal.MaterialEditingLibrary


def save(obj):
    obj.modify()
    if not globals().get('SAVE', True):      # SAVE = False: apply in memory only (another process holds the files)
        return
    # save_asset and save_loaded_asset report False on some of these packages; save_packages doesn't
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + obj.get_path_name())


def import_map(b, name):
    task = unreal.AssetImportTask()
    task.filename = b['model'] + name + '.png'
    task.destination_path = b['home']
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(f"{b['home']}/{name}")
    # exact 8-bit values, no mips (a lower mip would average neighbouring charts), clamped, never streamed
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('never_stream', True)
    save(tex)
    return tex


def own_instance(name, b):
    """MI_BF_<Name>_Concrete, a child of the shared instance, on the mesh's concrete slot (its import script puts the
    shared instance back: run this after it)."""
    path = f"{b['home']}/MI_BF_{name}_Concrete"
    if unreal.load_asset(path) is None:
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/', 1)[1], b['home'], unreal.MaterialInstanceConstant,
                                                                unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    EDIT.set_material_instance_parent(mi, unreal.load_asset(SHARED))
    mesh = unreal.load_asset(f"{b['home']}/{b['mesh']}")
    slots = [i for i, s in enumerate(mesh.get_editor_property('static_materials'))
             if str(s.get_editor_property('material_slot_name')).startswith(CONCRETE_SLOT)]
    if not slots:
        raise RuntimeError(f"{b['mesh']} has no {CONCRETE_SLOT} slot")
    for i in slots:
        mesh.set_material(i, mi)     # setting it on the struct only changes a copy
    save(mesh)
    return mi


def apply(name, b):
    mi = own_instance(name, b) if 'mesh' in b else unreal.load_asset(f"{b['home']}/MI_BF_{name}_Concrete")
    if mi is None:
        raise RuntimeError(f'no MI_BF_{name}_Concrete: run the building import or lamps script first')
    prefix = f'T_BF_{name}'
    EDIT.set_material_instance_static_switch_parameter_value(mi, 'BF_UseCurvatureMap', True)
    EDIT.set_material_instance_texture_parameter_value(mi, 'BF_CurvatureMap', import_map(b, prefix + '_Curv'))
    if b.get('fields'):
        meta = json.loads(open(f'{TOOLS}{prefix}_Fields.json', encoding='utf-8').read())
        EDIT.set_material_instance_static_switch_parameter_value(mi, 'BF_UseBakedFields', True)
        EDIT.set_material_instance_texture_parameter_value(mi, 'BF_FieldsMap', import_map(b, prefix + '_Fields'))
        EDIT.set_material_instance_scalar_parameter_value(mi, 'BF_FieldsLampScale', meta['lamp_scale'])
    if b.get('camo'):
        EDIT.set_material_instance_static_switch_parameter_value(mi, 'BF_UseCamoMap', True)
        EDIT.set_material_instance_texture_parameter_value(mi, 'BF_CamoMap', import_map(b, prefix + '_Camo'))
    EDIT.update_material_instance(mi)
    save(mi)
    print(name, 'maps on', mi.get_name())


if globals().get('SURFACE', True):
    shared = unreal.load_asset(SHARED)
    EDIT.set_material_instance_static_switch_parameter_value(shared, 'BF_UseBakedSurface', True)
    EDIT.update_material_instance(shared)
    save(shared)
    print('surface bake on', shared.get_name())
for building in globals().get('MAPS_TO_RUN', list(MAPS)):
    apply(building, MAPS[building])
