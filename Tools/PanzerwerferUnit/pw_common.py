"""Shared editor-side helpers for the Panzerwerfer 42 (Sd.Kfz. 4/1 Maultier) unit, 2026-11-17.
Sent into the live editor (UECP execute_python) concatenated before a job file (pw_import.py, pw_bp.py, ...).
No os / file access (sandbox). Every job saves only its own assets."""
import unreal

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SLIB = unreal.SubobjectDataBlueprintFunctionLibrary

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/PanzerWerfer/UE_Unit/'
EXP = SRC + 'Exports/'
TEXSRC = SRC + 'Textures/'
TV = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
PW = TV + 'Ger_ArmoredCar/Panzerwerfer'
MESHES = PW + '/Meshes'
ANIMS = PW + '/Anims'
MATS = PW + '/Materials'
TEXDIR = PW + '/Materials/Textures'

PZ38 = TV + 'Ger_Light/Panzer38T/'
SDKFZ250 = TV + 'Ger_ArmoredCar/Sdkfz-250/'
TURRET_DIR = '/Game/RTS_Survival/Blueprints/Turrets/German/Medium/PanzerWerfer/'

FULLUV_MASTER = TV + 'Ger_Medium/JagdPanzerIV70/Materials/MM_Ger_PostApoTank_NoBlend_FullUV'
JP_HULL_MI = TV + 'Ger_Medium/JagdPanzerIV70/Materials/MI_JagdPanzerIV70_Hull'
MM_WHEELS = TV + 'MasterMaterials/MM_Wheels'
MM_TRACKS = TV + 'MasterMaterials/MM_NormalTracks'
MM_ENV = '/Game/RTS_Survival/Materials/MasterMaterials/MM_RTS_Env'
BURNT_MASTER = TV + 'Ger_ArmoredCar/Keubel/Materials/M_Keubel_Destroyed'
LAUNCHER_MI = TURRET_DIR + 'Materials/MI_PanzerWerferLauncher'
LAUNCHER_D = TURRET_DIR + 'Materials/SdKfz_4_1_Launcher_gray_D'
LAUNCHER_N = TURRET_DIR + 'Materials/SdKfz_4_1_Launcher_N'
WHILE_CONSTRUCTING = '/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_WhileConstructing'
FLAT_NORMAL = '/Engine/EngineMaterials/FlatNormal'

# UE-space values from Exports/rig_info_panzerwerfer.json (UE Y = -Blender Y).
HULL_SOCKETS = {
    'Turret': (-185.0, 0.0, 172.5),      # launcher seat on the rear body roof
    'HullMg': (19.134, 50.898, 219.501),  # cab-ring MG pintle (right of the hatch)
    'Exhaust': (-60.0, 95.0, 45.0),      # Opel Blitz exhaust under the right running board
    'EngineFire': (175.0, 0.0, 150.0),   # engine under the armoured bonnet
}
MG_MUZZLE = (90.304, -4.524, 15.815)     # BulletLaunch0 on SM_PanzerWerfer_HullMg
WRECK_EXP1 = (-150.0, 0.0, 180.0)

ANIM_RENAMES = {
    'Anim_Tracks_PzWerfer_FastFWD': 'Anim_PanzerWerfer_Tracks_FWD',
    'Anim_Tracks_PzWerfer_FastBWD': 'Anim_PanzerWerfer_Tracks_BWD',
    'Anim_Tracks_PzWerfer_TPose': 'Anim_PanzerWerfer_Tracks_TPose',
    'Anim_Tracks_PzWerfer_TurnLeft': 'Anim_PanzerWerfer_Tracks_TurnLeft',
    'Anim_Tracks_PzWerfer_TurnRight': 'Anim_PanzerWerfer_Tracks_TurnRight',
    'Anim_Tracks_PzWerfer_TurnStationaryLeft': 'Anim_PanzerWerfer_Tracks_StationaryLeft',
    'Anim_Tracks_PzWerfer_TurnStationaryRight': 'Anim_PanzerWerfer_Tracks_StationaryRight',
}


def load(path):
    a = unreal.load_asset(path)
    if a is None:
        raise RuntimeError('missing ' + path)
    return a


def save(asset):
    if not EAL.save_loaded_asset(asset, False):
        print('SAVE FAILED', asset.get_path_name())
        return False
    return True


def assert_no_pie():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('PIE is running: not writing')


def dirty_names():
    ls = unreal.EditorLoadingAndSavingUtils
    return sorted(p.get_name() for p in ls.get_dirty_content_packages() + ls.get_dirty_map_packages())


def fbx_task(filename, dest_path, dest_name, ui):
    t = unreal.AssetImportTask()
    t.filename = filename
    t.destination_path = dest_path
    t.destination_name = dest_name
    t.replace_existing = True
    t.automated = True
    t.save = False
    t.factory = unreal.FbxFactory()
    t.options = ui
    return t


def static_ui():
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.import_as_skeletal = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    sm = ui.static_mesh_import_data
    sm.combine_meshes = True
    sm.auto_generate_collision = False
    sm.generate_lightmap_u_vs = True
    sm.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    sm.build_nanite = False
    return ui


def skeletal_ui():
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = True
    ui.import_as_skeletal = True
    ui.create_physics_asset = False
    ui.skeleton = None
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    sk = ui.skeletal_mesh_import_data
    sk.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    ui.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    return ui


def import_texture(png, name, normal=False):
    t = unreal.AssetImportTask()
    t.filename = TEXSRC + png
    t.destination_path = TEXDIR
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    AT.import_asset_tasks([t])
    tex = load(TEXDIR + '/' + name)
    if normal:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    save(tex)
    return tex


def make_mi(name, parent_path, textures=None, scalars=None, folder=MATS):
    path = folder + '/' + name
    mi = unreal.load_asset(path)
    if mi is None:
        mi = AT.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', load(parent_path))
    for k, v in (textures or {}).items():
        MEL.set_material_instance_texture_parameter_value(mi, k, load(v) if isinstance(v, str) else v)
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    save(mi)
    return mi


def set_static_slots(mesh, by_slot_name):
    mats = mesh.get_editor_property('static_materials')
    for i, m in enumerate(mats):
        slot = str(m.get_editor_property('material_slot_name'))
        if slot in by_slot_name:
            mesh.set_material(i, load(by_slot_name[slot]))
        else:
            print('  slot without mapping', mesh.get_name(), i, slot)


def add_static_socket(mesh, name, loc, rot=(0.0, 0.0, 0.0)):
    existing = mesh.find_socket(name)
    if existing:
        mesh.remove_socket(existing)
    s = unreal.StaticMeshSocket(mesh)
    s.set_editor_property('socket_name', name)
    s.set_editor_property('relative_location', unreal.Vector(*loc))
    s.set_editor_property('relative_rotation', unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]))
    mesh.add_socket(s)


def clear_static_sockets(mesh):
    for s in list(mesh.get_sockets_by_tag('')):
        mesh.remove_socket(s)


def handles(bp):
    out = {}
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        d = SDS.k2_find_subobject_data_from_handle(h)
        o = SLIB.get_object_for_blueprint(d, bp)
        if o is not None:
            out[o.get_name().replace('_GEN_VARIABLE', '')] = (h, d, o)
    return out


def template(bp, comp):
    return handles(bp)[comp][2]
