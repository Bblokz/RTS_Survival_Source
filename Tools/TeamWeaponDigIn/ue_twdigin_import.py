"""Imports the team weapon dig-in walls built by CustomMapItems/TeamWeaponDigIn/generate_tw_digin_walls.py (cut by
pit_mesh_optimize.py into SM_TWDigIn_<Gun>_Opt.fbx) as /Game/RTS_Survival/Meshes/Props/TeamWeaponDigIn/SM_TWDigIn_<Gun>.

ATeamWeapon's dig-in component finds its wall by that name (the gun Blueprint's class name). Per mesh:
  materials  the faction's sandbag and timber materials by slot name
  collision  one box per wall run from the layout (the slit stays open between boxes on slit pits), used for
             simple and complex queries: the bags' own triangles let a ray through the seam between two courses
  Nanite     off (the game does not render Nanite); LOD group LargeProp
  sockets    NavBox_<n>    a wall run: location = centre, scale = half extent / 100; the walls block navigation there
             GuardPost_<n> spots inside the pit for the gun's guards while dug in
             ProbeWall, ProbeSlit, Entrance, Inside   points for the dig-in map test

Run: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>" -unattended -nop4
Layout: tw_digin_layout.json next to this file (copied from the generator's Exports folder).
"""
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
LAYOUT = os.path.join(HERE, 'tw_digin_layout.json')
EXPORTS = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/TeamWeaponDigIn/Exports'
DESTINATION = '/Game/RTS_Survival/Meshes/Props/TeamWeaponDigIn'
MATERIALS = {
    'TWDigIn_GerSandbag': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
    'TWDigIn_GerTimber': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/M_GerCamoConcretePalet_Atlas',
    'TWDigIn_RusSandbag': '/Game/SandBagPack/Materials/MI_Rus_SandBag_01',
    'TWDigIn_RusTimber': '/Game/RTS_Survival/StarterContent/Materials/M_Wood_Pine',
}
SOCKET_SCALE = 100.0
eal = unreal.EditorAssetLibrary


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
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('build_nanite', False)
    # The cut mesh carries the full mesh's normals; they must not be recomputed.
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def import_mesh(name):
    task = unreal.AssetImportTask()
    task.filename = f'{EXPORTS}/{name}_Opt.fbx'
    task.destination_path = DESTINATION
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = static_options()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(f'{DESTINATION}/{name}')
    if mesh is None:
        raise RuntimeError('Import produced nothing for ' + name)
    return mesh


def set_materials(mesh):
    for index, slot in enumerate(mesh.static_materials):
        target = MATERIALS.get(str(slot.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
        else:
            print('TWDIGIN_IMPORT unmatched slot', slot.material_slot_name)


def nanite_off(mesh):
    # Through the subsystem so the mesh rebuilds: setting the property alone leaves the Nanite fallback in place.
    settings = mesh.get_editor_property('nanite_settings')
    settings.enabled = False
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, apply_changes=True)


def add_socket(mesh, name, location, scale=(1.0, 1.0, 1.0)):
    socket = unreal.StaticMeshSocket(mesh)
    socket.set_editor_property('socket_name', name)
    socket.set_editor_property('relative_location', unreal.Vector(*location))
    socket.set_editor_property('relative_scale', unreal.Vector(*scale))
    mesh.add_socket(socket)


def set_collision_boxes(mesh, pit):
    body = mesh.get_editor_property('body_setup')
    geometry = body.get_editor_property('agg_geom')
    boxes = []
    for entry in pit['collision_boxes']:
        box = unreal.KBoxElem()
        box.set_editor_property('center', unreal.Vector(*entry['centre']))
        box.set_editor_property('x', entry['half'][0] * 2.0)
        box.set_editor_property('y', entry['half'][1] * 2.0)
        box.set_editor_property('z', entry['half'][2] * 2.0)
        boxes.append(box)
    geometry.set_editor_property('box_elems', boxes)
    geometry.set_editor_property('convex_elems', [])
    body.set_editor_property('agg_geom', geometry)
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    return len(boxes)


def set_sockets(mesh, pit):
    for socket in list(mesh.get_sockets_by_tag('')):
        mesh.remove_socket(socket)
    for index, box in enumerate(pit['nav_boxes']):
        add_socket(mesh, f'NavBox_{index}', box['centre'], [extent / SOCKET_SCALE for extent in box['half']])
    for index, post in enumerate(pit['guard_posts']):
        add_socket(mesh, f'GuardPost_{index}', post)
    for name, location in pit['probes'].items():
        add_socket(mesh, name, location)


def import_pit(gun, pit):
    name = pit['mesh']
    mesh = import_mesh(name)
    set_materials(mesh)
    box_count = set_collision_boxes(mesh, pit)
    mesh.set_editor_property('lod_group', 'LargeProp')
    set_sockets(mesh, pit)
    nanite_off(mesh)
    path = f'{DESTINATION}/{name}'
    saved = eal.save_asset(path, only_if_is_dirty=False)
    mesh = unreal.load_asset(path)
    box = mesh.get_bounding_box()
    print('TWDIGIN_IMPORT', gun, 'tris', [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
          'nanite', mesh.get_editor_property('nanite_settings').enabled,
          'materials', [m.material_interface.get_name() if m.material_interface else None for m in mesh.static_materials],
          'sockets', len(mesh.get_sockets_by_tag('')), 'boxes', box_count,
          'collision_boxes_now', len(mesh.get_editor_property('body_setup').get_editor_property('agg_geom').get_editor_property('box_elems')),
          'box', [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)], 'saved', saved)


def main():
    with open(LAYOUT, encoding='utf-8') as handle:
        layout = json.load(handle)
    for gun, pit in layout.items():
        import_pit(gun, pit)
    print('TWDIGIN_IMPORT done', len(layout))


main()
