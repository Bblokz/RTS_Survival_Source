"""Desert camouflage net variants for the owner to choose from (round 5; the default stays the faction camo).
MI_Rus_CamoNet_Desert and MI_Rus_CamoNet_Desert_Opaque: the tent camo materials with the Russian pattern's desert texture.
Only the base kit's net meshes take them, and only under -SovietBaseDesertNets (map-test builds). Headless, re-runnable."""
import unreal

FOLDER = '/Game/RTS_Survival/SurvivalMode/Data/SovietBases/Foundations'
MATERIALS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/FactoryMeshes/Materials/'
DESERT = MATERIALS + 'T_RUS_Pattern_1_DesertCamo'
lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def make(name, parent_path):
    path = FOLDER + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    parent = unreal.load_asset(parent_path)
    inst = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    inst.set_editor_property('parent', parent)
    names = lib.get_texture_parameter_names(parent)
    print('SOVIET_NET', name, 'parent params', list(names))
    if 'CamoDiff' in [str(n) for n in names]:
        lib.set_material_instance_texture_parameter_value(inst, 'CamoDiff', unreal.load_asset(DESERT))
    lib.update_material_instance(inst)
    print('SOVIET_NET saved', name, unreal.EditorAssetLibrary.save_loaded_asset(inst, only_if_is_dirty=False))


make('MI_Rus_CamoNet_Desert', MATERIALS + 'M_Russian_CamoPattern_Tent')
make('MI_Rus_CamoNet_Desert_Opaque', MATERIALS + 'M_Russian_CamoPattern_Tent_Opaque_Regular')
