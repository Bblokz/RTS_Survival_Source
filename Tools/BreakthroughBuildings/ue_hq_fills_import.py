"""Imports the Breakthrough HQ's resource fills (run with the UECP execute_python action).

Blender source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/HQ/Model/hq_fills.py
(see HQ/Model/README.md). 15 meshes, SM_BF_HQ_<Radixite|Metal|VehicleParts>At<10|25|50|75|100>, pivot on their
socket. Radixite: the game's crystals on MI_Crystal1. Metal and vehicle parts: one material on an 8 x 4 pixel palette, every
face's UVs on one pixel centre.

Safe to rerun after a Blender rebuild: meshes and textures reimport over the same assets, the material is rebuilt.
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/HQ/Model/Fills/'
HOME = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughHQ/Fills'
MATERIAL = HOME + '/M_BF_ResourceFill'
# The radixite store is built from the game's crystals with their UVs and wears the resource's own material.
CRYSTAL_MATERIAL = '/Game/RTS_Survival/Blueprints/Resources/Crystals/Textures/MI_Crystal1'
RESOURCES = ('Radixite', 'Metal', 'VehicleParts')
LEVELS = (10, 25, 50, 75, 100)
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def save(path):
    if not ASSETS.save_loaded_asset(unreal.load_asset(path), False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):    # a save can report success and still miss the disk
        raise RuntimeError('not on disk ' + disk)


def import_texture(name, srgb):
    task = unreal.AssetImportTask()
    task.filename = MODEL + 'Textures/' + name + '.png'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    ASSET_TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{HOME}/{name}')
    # one pixel per swatch: block compression, filtering or mips would bleed neighbours into each other
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('srgb', srgb)
    tex.set_editor_property('never_stream', True)
    save(f'{HOME}/{name}')
    return tex


def build_material(bc, rm):
    if ASSETS.does_asset_exist(MATERIAL):
        mat = unreal.load_asset(MATERIAL)
        MEL.delete_all_material_expressions(mat)
    else:
        mat = ASSET_TOOLS.create_asset('M_BF_ResourceFill', HOME, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y):
        return MEL.create_material_expression(mat, cls, x, y)

    bc_node = node(unreal.MaterialExpressionTextureSample, -900, -200)
    bc_node.set_editor_property('texture', bc)
    bc_node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    rm_node = node(unreal.MaterialExpressionTextureSample, -900, 250)
    rm_node.set_editor_property('texture', rm)
    rm_node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)

    # +-12 % brightness from world-space noise, so the flat swatches don't read as CG
    world = node(unreal.MaterialExpressionWorldPosition, -1100, -480)
    noise = node(unreal.MaterialExpressionNoise, -900, -480)
    noise.set_editor_property('scale', 0.009)
    noise.set_editor_property('levels', 2)
    noise.set_editor_property('output_min', 0.88)
    noise.set_editor_property('output_max', 1.12)
    MEL.connect_material_expressions(world, '', noise, 'Position')
    tint = node(unreal.MaterialExpressionMultiply, -500, -300)
    MEL.connect_material_expressions(bc_node, 'RGB', tint, 'A')
    MEL.connect_material_expressions(noise, '', tint, 'B')
    MEL.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)

    # alpha marks glowing swatches (none in use since radixite moved to MI_Crystal1)
    ember = node(unreal.MaterialExpressionVectorParameter, -900, -20)
    ember.set_editor_property('parameter_name', 'EmberColor')
    ember.set_editor_property('default_value', unreal.LinearColor(1.0, 0.28, 0.05, 1.0))
    strength = node(unreal.MaterialExpressionScalarParameter, -700, 80)
    strength.set_editor_property('parameter_name', 'EmberStrength')
    strength.set_editor_property('default_value', 6.0)
    glow = node(unreal.MaterialExpressionMultiply, -500, 0)
    MEL.connect_material_expressions(bc_node, 'A', glow, 'A')
    MEL.connect_material_expressions(ember, '', glow, 'B')
    glow2 = node(unreal.MaterialExpressionMultiply, -300, 40)
    MEL.connect_material_expressions(glow, '', glow2, 'A')
    MEL.connect_material_expressions(strength, '', glow2, 'B')
    MEL.connect_material_property(glow2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.connect_material_property(rm_node, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(rm_node, 'G', unreal.MaterialProperty.MP_METALLIC)
    MEL.recompile_material(mat)
    save(MATERIAL)
    return mat


def import_mesh(name):
    """Legacy FBX, as the HQ itself: no collision (the component has none), no lightmap UVs."""
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
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    ASSET_TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{HOME}/{name}')
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        # a reimport over the old oxide meshes keeps their palette slot, empty now: give it the crystal material too
        crystal = name.startswith('SM_BF_HQ_Radixite') or str(slot.get_editor_property('material_slot_name')).startswith('MI_Crystal1')
        # setting it on the struct only changes a copy
        mesh.set_material(index, unreal.load_asset(CRYSTAL_MATERIAL if crystal else MATERIAL))
    save(f'{HOME}/{name}')
    box = mesh.get_bounding_box()
    return mesh.get_num_triangles(0), [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)]


def main():
    bc = import_texture('T_BF_FillPalette_BC', True)
    rm = import_texture('T_BF_FillPalette_RM', False)
    build_material(bc, rm)
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for res in RESOURCES:
            for level in LEVELS:
                name = f'SM_BF_HQ_{res}At{level}'
                tris, bounds = import_mesh(name)
                print(name, 'tris', tris, 'bounds', bounds)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')


main()
