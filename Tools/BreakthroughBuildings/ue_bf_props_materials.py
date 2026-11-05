"""Breakthrough faction props materials (run with the UECP execute_python action).

The faction's crates, barrels and tanks use the airport props atlases (T_Props_1 / T_Props_2) with the Dunkelgelb base colour the
user picked on 2026-11-02 (Blender: AssetsForFactionIdentity/Props, gen_options.py, option B). The airport's own MI_Props_1 / MI_Props_2
keep the original art for every other prop in the game (user, 2026-11-05), so the faction gets its own instances:

  BreakthroughShared/Textures/T_BF_Props_1_BC, T_BF_Props_2_BC   the Dunkelgelb base colours
  BreakthroughShared/MI_BF_Props_1, MI_BF_Props_2                 copies of MI_Props_1 / MI_Props_2 (same M_General parent, normal and
                                                                  DRO mask) with only BaseColor swapped

and every Breakthrough mesh slot on MI_Props_1 / MI_Props_2 moves to them. The building import scripts map the T_Props_* slots to these
instances too, so a reimport keeps them. Safe to rerun: textures reimport in place, existing instances are updated.
"""
import unreal

BLEND = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/AssetsForFactionIdentity/Props/'
AIRPORT = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/Materials'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared'
CHILD = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/'
ATLASES = ('1', '2')
ASSETS = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def save(path):
    obj = unreal.load_asset(path)
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):    # a save can report success and still miss the disk
        raise RuntimeError('not on disk ' + disk)


def import_base_colour(atlas):
    """The Dunkelgelb base colour, with the airport texture's settings so both atlases filter and stream alike."""
    name = f'T_BF_Props_{atlas}_BC'
    task = unreal.AssetImportTask()
    task.filename = f'{BLEND}T_Props_{atlas}_BC.PNG'
    task.destination_path = SHARED + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False    # AssetImportTask(save=True) silently misses the disk here
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(f'{SHARED}/Textures/{name}')
    original = unreal.load_asset(f'{AIRPORT}/Textures/T_Props_{atlas}_BC')
    for prop in ('srgb', 'compression_settings', 'lod_group', 'mip_gen_settings', 'max_texture_size'):
        tex.set_editor_property(prop, original.get_editor_property(prop))
    save(f'{SHARED}/Textures/{name}')
    return tex


def instance(atlas, tex):
    path = f'{SHARED}/MI_BF_Props_{atlas}'
    if not ASSETS.does_asset_exist(path):
        if ASSETS.duplicate_asset(f'{AIRPORT}/MI_Props_{atlas}', path) is None:
            raise RuntimeError('could not copy MI_Props_' + atlas)
    mi = unreal.load_asset(path)
    EDIT.set_material_instance_texture_parameter_value(mi, 'BaseColor', tex)
    EDIT.update_material_instance(mi)
    save(path)
    return mi


def repoint(replacements):
    """Every Breakthrough static mesh slot on an airport props instance moves to the faction's copy."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for old_path, new in replacements.items():
        for ref in registry.get_referencers(old_path, unreal.AssetRegistryDependencyOptions()):
            ref = str(ref)
            if not ref.startswith(CHILD + 'Breakthrough'):
                continue
            mesh = unreal.load_asset(ref)
            if not isinstance(mesh, unreal.StaticMesh):
                continue
            slots = [i for i in range(len(mesh.get_editor_property('static_materials')))
                     if mesh.get_material(i) and mesh.get_material(i).get_path_name().split('.')[0] == old_path]
            for i in slots:
                mesh.set_material(i, new)     # setting it on the struct only changes a copy
            if slots:
                save(ref)
                print('repointed', ref.split('/')[-1], slots, '->', new.get_name())


def main():
    replacements = {}
    for atlas in ATLASES:
        mi = instance(atlas, import_base_colour(atlas))
        replacements[f'{AIRPORT}/MI_Props_{atlas}'] = mi
        print(mi.get_name(), 'BaseColor', EDIT.get_material_instance_texture_parameter_value(mi, 'BaseColor').get_name())
    repoint(replacements)


main()
