"""Puts the German pit trench anchors (Docs/Units/GerPitTrenchAnchors.md) into the game.

Blender side: GerDefenses/Scripts/gerpit_trench_anchors.py writes Exports/TrenchAnchors/<Pit>/:
  SM_GerPit_<Pit>[_Opt].fbx          the pit (only reimported when its wall or props changed; the _Opt file is the
                                      pit_mesh_optimize.py cut, as for every pit mesh, see Docs/Units/GunPitMeshes.md)
  SM_GerPit_<Pit>_TrenchNeck_*.fbx   the necks, SM_GerPit_<Pit>_TrenchPlug_*.fbx the plugs (origin on the port)

Per pit this script:
  - reimports the pit over its asset when REIMPORT_PIT is set, keeping material per slot, collision flag and LOD group,
    with Nanite off (as Tools/GunPits/optimize_pit_meshes.py does);
  - sets every TrenchPort_<Anchor> socket to its pure-yaw transform (an FBX socket empty imports pitched 90 degrees);
  - imports the necks and plugs into GerPits/Meshes: the pit's materials by slot name, complex collision, LOD group
    LargeProp, no Nanite.
Unreal values are Blender's with Y mirrored (yaw negated).

Run inside the editor (execute_python) with PITS_TO_RUN set, e.g. PITS_TO_RUN = ['Flak', 'Howitzer'].
"""
import unreal

EXPORTS = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/TrenchAnchors'
MESHES = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes'

SLOT_MATERIALS = {
    'GerPit_Atlas': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/M_GerCamoConcretePalet_Atlas',
    'GerPit_Sandbag': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
}
TWIN_MESHES ='/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/TwinMG42Pit/Meshes'

# Pit key -> (asset folder, asset, FBX the pit is reimported from (None: only the sockets change),
#             {socket: (x, y, yaw)} in Unreal space, extra meshes imported beside the pit)
PITS = {
    'Pak': (MESHES, 'SM_GerPit_Pak', None, {'TrenchPort_Rear': (-528.0, 0.0, 180.0)},
            ['SM_GerPit_Pak_TrenchNeck_Rear']),
    'Flak': (MESHES, 'SM_GerPit_Flak', 'SM_GerPit_Flak_Opt', {'TrenchPort_Rear': (-558.0, 0.0, 180.0)},
             ['SM_GerPit_Flak_TrenchNeck_Rear']),
    'Howitzer': (MESHES, 'SM_GerPit_Howitzer', 'SM_GerPit_Howitzer_Opt',
                 {'TrenchPort_Rear': (-648.0, 0.0, 180.0),
                  'TrenchPort_RearLeft': (-458.21, -458.21, -135.0),
                  'TrenchPort_RearRight': (-458.21, 458.21, 135.0)},
                 ['SM_GerPit_Howitzer_TrenchNeck_Rear', 'SM_GerPit_Howitzer_TrenchNeck_Side',
                  'SM_GerPit_Howitzer_TrenchPlug_RearLeft', 'SM_GerPit_Howitzer_TrenchPlug_RearRight']),
    'Mortar': (MESHES, 'SM_GerMortarPit', None, {'TrenchPort_RearRight': (-388.5, 405.0, 90.0)},
               ['SM_GerMortarPit_TrenchNeck_RearRight']),
    # The twin MG42 pit's asset is named apart from its FBX.
    'TwinMG42': (TWIN_MESHES, 'SM_GerPit_TwinMG42_Sandbags', 'SM_GerTwinMG42Pit_Opt',
                 {'TrenchPort_Left': (40.0, -655.0, -90.0), 'TrenchPort_Right': (40.0, 655.0, 90.0)},
                 ['SM_GerTwinMG42Pit_TrenchNeck_Left', 'SM_GerTwinMG42Pit_TrenchNeck_Right',
                  'SM_GerTwinMG42Pit_TrenchPlug_Left', 'SM_GerTwinMG42Pit_TrenchPlug_Right']),
    # The Flak 88 emplacement's bags are already its section's lightest; it is not cut.
    'Flak88': (MESHES, 'SM_GerFlak88Pit', 'SM_GerFlak88Pit',
               {'TrenchPort_Rear': (-536.5, 110.0, 180.0), 'TrenchPort_Right': (-200.0, 536.5, 90.0)},
               ['SM_GerFlak88Pit_TrenchNeck_Rear', 'SM_GerFlak88Pit_TrenchNeck_Right', 'SM_GerFlak88Pit_TrenchPlug_Right']),
}
# Plugs are wall bags at the generator's full detail; like the pit's own bags they are cut first (pit_mesh_optimize.py,
# GerPit_Sandbag=0.4) and imported from the _Opt file.
CUT_EXTRAS = {'SM_GerPit_Howitzer_TrenchPlug_RearLeft', 'SM_GerPit_Howitzer_TrenchPlug_RearRight',
              'SM_GerTwinMG42Pit_TrenchPlug_Left', 'SM_GerTwinMG42Pit_TrenchPlug_Right'}
eal = unreal.EditorAssetLibrary


def _options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    data = options.static_mesh_import_data
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('build_nanite', False)
    data.set_editor_property('generate_lightmap_u_vs', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def _import(fbx, folder, name):
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = _options()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(f'{folder}/{name}')
    if mesh is None:
        raise RuntimeError('Import produced nothing for ' + name)
    return mesh


def _nanite_off(mesh):
    settings = mesh.get_editor_property('nanite_settings')
    settings.enabled = False
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, apply_changes=True)


def _apply_materials(mesh, materials):
    for index, slot in enumerate(mesh.static_materials):
        material = materials.get(str(slot.material_slot_name))
        if material is None:
            print('TRENCH_ANCHOR unmatched slot', mesh.get_name(), slot.material_slot_name)
            continue
        mesh.set_material(index, material)


def _set_sockets(mesh, sockets):
    for name, (x, y, yaw) in sockets.items():
        socket = mesh.find_socket(name)
        if socket is None:
            socket = unreal.StaticMeshSocket(mesh)
            socket.set_editor_property('socket_name', name)
            mesh.add_socket(socket)
        else:
            imported = socket.relative_location
            print('TRENCH_ANCHOR imported', name, 'at', round(imported.x, 1), round(imported.y, 1),
                  round(imported.z, 1), 'expected', x, y)
        socket.set_editor_property('relative_location', unreal.Vector(x, y, 0.0))
        socket.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        socket.set_editor_property('relative_scale', unreal.Vector(1.0, 1.0, 1.0))
    mesh.modify()


def run_pit(key, necks_only=False):
    """necks_only (NECKS_ONLY = True): only the TrenchNeck meshes are imported; the pit, its sockets and the plugs are
    left alone. Used after the field trench kit changed width (2026-11-09)."""
    folder, asset, reimport_fbx, sockets, extras = PITS[key]
    mesh = unreal.load_asset(f'{folder}/{asset}')
    # Necks need the pallet atlas even where the pit itself has no atlas slot (twin MG42).
    materials = {name: unreal.load_asset(path) for name, path in SLOT_MATERIALS.items()}
    materials.update({str(s.material_slot_name): s.material_interface for s in mesh.static_materials})
    if necks_only:
        extras = [name for name in extras if '_TrenchNeck_' in name]
    elif reimport_fbx:
        collision = mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag')
        lod_group = mesh.get_editor_property('lod_group')
        mesh = _import(f'{EXPORTS}/{key}/{reimport_fbx}.fbx', folder, asset)
        _apply_materials(mesh, materials)
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', collision)
        if str(lod_group) != 'None':
            mesh.set_editor_property('lod_group', lod_group)
        _nanite_off(mesh)
    if not necks_only:
        _set_sockets(mesh, sockets)
        print('TRENCH_ANCHOR pit', asset, 'tris', mesh.get_num_triangles(0), 'nanite',
              mesh.get_editor_property('nanite_settings').enabled, 'saved', eal.save_loaded_asset(mesh, False))
    for name in extras:
        suffix = '_Opt' if name in CUT_EXTRAS else ''
        extra = _import(f'{EXPORTS}/{key}/{name}{suffix}.fbx', folder, name)
        _apply_materials(extra, materials)
        extra.get_editor_property('body_setup').set_editor_property(
            'collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        extra.set_editor_property('lod_group', 'LargeProp')
        _nanite_off(extra)
        box = extra.get_bounding_box()
        print('TRENCH_ANCHOR mesh', name, 'tris', extra.get_num_triangles(0), 'box',
              [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
              'saved', eal.save_loaded_asset(extra, False))


for _key in globals().get('PITS_TO_RUN') or []:
    run_pit(_key, bool(globals().get('NECKS_ONLY')))
