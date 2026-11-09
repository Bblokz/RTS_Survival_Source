"""Puts the Soviet pit trench anchors (Docs/Units/SovPitTrenchAnchors.md) into the game.

Blender side: one folder per pit under CustomMapItems/SovietsStuff/TrenchAnchors/<Key>/ with the neck FBX and an
anchors.json (socket, Unreal location/yaw, neck mesh). Copy every anchors.json to Source/Tools/SovPits/anchors/<Key>.json
before running (the editor's sandboxed Python reads only project files).

Per pit this script:
  - adds (or moves) each TrenchPort_<Name> socket on the pit's static mesh, pure yaw; the pit geometry is not touched;
  - imports each neck next to the pit mesh: slots SovTrench_Wood -> M_SovFieldTrench_Wood, RL_Sandbag ->
    MI_Rus_SandBag_01, complex collision, LOD group LargeProp, no Nanite (the German neck settings);
  - adds a TrenchAnchors component (UFieldTrenchAnchorComponent) to the pit's building expansion Blueprint and lists
    each socket's neck (no plugs: every Soviet anchor uses an existing crew entrance).

Run inside an editor that has UFieldTrenchAnchorComponent (execute_python) with PITS_TO_RUN set, e.g.
PITS_TO_RUN = ['53KPit'], or unset for all.
"""
import json

import unreal

ANCHOR_DIR = 'D:/UE5Projects/RTS_Survival/Source/Tools/SovPits/anchors'
BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions'
SLOT_MATERIALS = {
    'SovTrench_Wood': '/Game/RTS_Survival/Blueprints/Environment/Trenches/SovFieldTrench/Materials/M_SovFieldTrench_Wood',
    'RL_Sandbag': '/Game/SandBagPack/Materials/MI_Rus_SandBag_01',
}
COMPONENT_NAME = 'TrenchAnchors'
# Pit key -> (pit mesh asset, building expansion Blueprint)
PITS = {
    '53KPit': (BXP + '/Rus53KPit/Meshes/SM_53KPit', BXP + '/Rus53KPit/Rus_BXP_53KPit'),
    '53KSectorPit': (BXP + '/Rus53KPit/Meshes/SM_53KSectorPit', BXP + '/Rus53KPit/Rus_BXP_53KSectorPit'),
    'BS3Pit': (BXP + '/RusBS3Pit/Meshes/SM_BS3Pit', BXP + '/RusBS3Pit/Rus_BXP_BS3Pit'),
    'DShKPit': (BXP + '/RusDShKPit/Meshes/SM_DShKPit', BXP + '/RusDShKPit/Rus_BXP_DShKPit'),
    'AmpulometPit': (BXP + '/RusAmpulometPit/Meshes/SM_AmpulometPit', BXP + '/RusAmpulometPit/Rus_BXP_AmpulometPit'),
    'Rus120MortarPit': (BXP + '/RusMortarPit/Meshes/SM_Rus120MortarPit', BXP + '/RusMortarPit/Rus_BXP_120MortarPit'),
}
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

try:
    PITS_TO_RUN
except NameError:
    PITS_TO_RUN = list(PITS)


def set_socket(mesh, name, location, yaw):
    socket = mesh.find_socket(name)
    if socket is None:
        socket = unreal.StaticMeshSocket(mesh)
        socket.set_editor_property('socket_name', name)
        mesh.add_socket(socket)
        socket = mesh.find_socket(name)
    socket.set_editor_property('relative_location', unreal.Vector(*location))
    socket.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    socket.set_editor_property('relative_scale', unreal.Vector(1.0, 1.0, 1.0))
    mesh.modify()


def static_options():
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
    data.set_editor_property('generate_lightmap_u_vs', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def import_neck(fbx, folder, name):
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = static_options()
    asset_tools.import_asset_tasks([task])
    path = folder + '/' + name
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError('import produced nothing for ' + fbx)
    for index, slot in enumerate(mesh.static_materials):
        target = SLOT_MATERIALS.get(str(slot.material_slot_name))
        if target is None:
            raise RuntimeError('%s: unexpected slot %s' % (name, slot.material_slot_name))
        mesh.set_material(index, unreal.load_asset(target))
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_editor_property('lod_group', 'LargeProp')
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(
        mesh, unreal.MeshNaniteSettings(enabled=False), apply_changes=True)
    print('SOVPIT_NECK', name, 'tris', mesh.get_num_triangles(0), 'saved', eal.save_loaded_asset(mesh, False))
    return path


def find_or_add_component(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    for handle in handles:
        template = library.get_object_for_blueprint(library.get_data(handle), blueprint)
        if isinstance(template, unreal.FieldTrenchAnchorComponent):
            return template
    params = unreal.AddNewSubobjectParams(
        parent_handle=handles[0], new_class=unreal.FieldTrenchAnchorComponent, blueprint_context=blueprint)
    new_handle, fail_reason = subsystem.add_new_subobject(params)
    if not fail_reason.is_empty():
        raise RuntimeError('could not add the component: ' + str(fail_reason))
    subsystem.rename_subobject(new_handle, unreal.Text(COMPONENT_NAME))
    return library.get_object_for_blueprint(library.get_data(new_handle), blueprint)


def make_definition(socket, neck_path):
    name = neck_path.rsplit('/', 1)[1]
    definition = unreal.FieldTrenchAnchorDefinition()
    definition.import_text('(SocketName="%s",NeckMesh="%s.%s",PlugMesh=None)' % (socket, neck_path, name))
    return definition


def run_pit(key):
    mesh_path, blueprint_path = PITS[key]
    with open('%s/%s.json' % (ANCHOR_DIR, key)) as handle:
        record = json.load(handle)
    if record.get('pit_geometry_changed'):
        raise RuntimeError(key + ': the pit geometry changed; reimport the pit first (not handled here)')
    mesh = unreal.load_asset(mesh_path)
    folder = mesh_path.rsplit('/', 1)[0]
    definitions = []
    for anchor in record['anchors']:
        x, y, z = anchor['unreal_location']
        set_socket(mesh, anchor['socket'], (x, y, z), anchor['unreal_yaw'])
        neck_path = import_neck(anchor['neck_fbx'], folder, anchor['neck_mesh_name'])
        definitions.append(make_definition(anchor['socket'], neck_path))
    print('SOVPIT_SOCKETS', key, [str(s.socket_name) for s in mesh.get_sockets_by_tag('')],
          'saved', eal.save_loaded_asset(mesh, False))
    blueprint = unreal.load_asset(blueprint_path)
    template = find_or_add_component(blueprint)
    template.set_editor_property('anchors', definitions)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    saved = eal.save_loaded_asset(blueprint, False)
    print('SOVPIT_BP', key, 'saved', saved, [d.export_text() for d in template.get_editor_property('anchors')])


for pit_key in PITS_TO_RUN:
    run_pit(pit_key)
