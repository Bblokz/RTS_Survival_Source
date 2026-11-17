"""Imports the Oder-Warthe Werk bunker meshes (Blender: CustomMapItems/OderWarthe/Werke, README there) into
/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Werke, with the Breakthrough faction's concrete.

Each mesh gets its own concrete instance MI_OW_Werk_<Name>_Concrete under BreakthroughShared/MI_BF_SootedFortress_UVTile (the
mesh holds the 4 m Sooted Fortress tile in its UVs) with its baked curvature map (BF_UseCurvatureMap / BF_CurvatureMap, the
rebar gate), and the faction trim M_BF_Trim on the trim slot, as ue_building_maps_import.py does for the faction buildings.
Legacy FBX import (Interchange drops UCX hulls). Safe to rerun after a Blender rebuild.

Run in the build editor:  python Source/Tools/OderWartheMap/ow_cmd.py Source/Tools/OderWartheMap/ow_werk_import.py
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Werke/Model/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Werke'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/'
NAMES = ('Kasemattennase', 'Flankierend', 'Peilturm')
CONCRETE_SLOT = 'M_BF_SootedFortress_Concrete'
SOCKETS = ('HQ_Command', 'HQ_Transmitter', 'HQ_Rail', 'Entrance', 'Tobruk1', 'Tobruk2', 'Tobruk3', 'Leitstand1', 'Leitstand2',
           'PeilturmTop', 'Observation1', 'Observation2', 'Observation3')
EDIT = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def save(obj):
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + obj.get_path_name())


def import_mesh(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = f'{MODEL}{name}/{name}.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    TOOLS.import_asset_tasks([task])
    return unreal.load_asset(f'{HOME}/{name}')


def import_curv(name, short):
    tname = f'T_OW_Werk_{short}_Curv'
    task = unreal.AssetImportTask()
    task.filename = f'{MODEL}{name}/{tname}.png'
    task.destination_path = HOME
    task.destination_name = tname
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{HOME}/{tname}')
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


def concrete_instance(short, curv):
    path = f'{HOME}/MI_OW_Werk_{short}_Concrete'
    if unreal.load_asset(path) is None:
        TOOLS.create_asset(path.rsplit('/', 1)[1], HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    EDIT.set_material_instance_parent(mi, unreal.load_asset(SHARED + 'MI_BF_SootedFortress_UVTile'))
    EDIT.set_material_instance_static_switch_parameter_value(mi, 'BF_UseCurvatureMap', True)
    EDIT.set_material_instance_texture_parameter_value(mi, 'BF_CurvatureMap', curv)
    EDIT.update_material_instance(mi)
    save(mi)
    return mi


def finish(mesh, mi):
    trim = unreal.load_asset(SHARED + 'M_BF_Trim')
    slots = mesh.get_editor_property('static_materials')
    for i, slot in enumerate(slots):
        name = str(slot.get_editor_property('material_slot_name'))
        mesh.set_material(i, mi if name.startswith(CONCRETE_SLOT) else trim)
    found = []
    for socket_name in SOCKETS:     # FBX empties arrive tilted: keep the positions only ('Sockets' is protected in Python)
        socket = mesh.find_socket(socket_name)
        if socket:
            socket.set_editor_property('relative_rotation', unreal.Rotator(0, 0, 0))
            socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
            found.append(socket_name)
    save(mesh)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    print(mesh.get_name(), 'triangles', mesh.get_num_triangles(0), 'convex hulls', hulls, 'slots',
          [str(s.get_editor_property('material_slot_name')) for s in slots],
          'sockets', found)


unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
try:
    for short in NAMES:
        name = 'SM_OW_Werk_' + short
        mesh = import_mesh(name)
        mi = concrete_instance(short, import_curv(name, short))
        finish(mesh, mi)
finally:
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
