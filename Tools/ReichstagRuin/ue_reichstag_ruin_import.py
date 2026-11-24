"""Imports the Reichstag ruin kit and assembles the approved B2 layout (run with the UECP execute_python action).

Blender source, piece list and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/BerlinReichstagBase/
README.md (Model/rt_build.py writes the FBX files and RT_Layout.json). Only the ruin is modelled; the Soviet factory,
barracks and pits are the existing Blueprints and are placed on the map, not in this actor.

Makes, under /Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin:
- Mesh/SM_RT_*            20 kit pieces, legacy FBX import (keeps the UCX hulls), Nanite off
- the slot materials: look A Sandstein, picked by the user 2026-11-12 (Materials/Looks, built by ue_reichstag_looks.py on the
  Brandenburg Gate's sandstone master, so both landmarks match); steel is the Gate's rebar rust
- BP_ReichstagRuin_B2     one actor: an InstancedStaticMeshComponent per piece, instances from RT_Layout.json
Safe to rerun: meshes reimport in place, the Blueprint's instances are rebuilt from the layout.
"""
import json

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/BerlinReichstagBase/Model/'
# rt_build.py also writes the layout here: the editor's Python may only read files inside the project
LAYOUT = 'D:/UE5Projects/RTS_Survival/Source/Tools/ReichstagRuin/RT_Layout.json'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin'
MESH = ROOT + '/Mesh'
MATS = ROOT + '/Materials'
BP_NAME = 'BP_ReichstagRuin_B2'
PIECES = ['SM_RT_Wall_H04_A', 'SM_RT_Wall_H04_B', 'SM_RT_Wall_H08_A', 'SM_RT_Wall_H08_B', 'SM_RT_Wall_H12_A',
          'SM_RT_Wall_H12_B', 'SM_RT_Wall_H16_A', 'SM_RT_Wall_H16_B', 'SM_RT_Wall_End_R', 'SM_RT_Wall_End_L',
          'SM_RT_Tower_Tall', 'SM_RT_Tower_Mid', 'SM_RT_Tower_Low', 'SM_RT_Portico', 'SM_RT_Dome', 'SM_RT_DomeHook',
          'SM_RT_Rubble_S', 'SM_RT_Rubble_M', 'SM_RT_Rubble_L', 'SM_RT_Rubble_Ramp']
# slot name in the FBX -> material (look A Sandstein; run ue_reichstag_looks.py first to (re)make the instances)
SLOT_MATERIALS = {
    'RT_Stone': MATS + '/Looks/MI_RT_LookA_Stone',
    'RT_Rubble': MATS + '/Looks/MI_RT_LookA_Rubble',
    'RT_Steel': '/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate/MI_BG_RebarRust',
}
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
SUBOBJECTS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def save(path):
    if not ASSETS.save_loaded_asset(unreal.load_asset(path), False):
        print('SAVE FAILED', path)


def slot_materials():
    """The kit is drawn by ISM components: each parent material needs the instanced-static-mesh usage saved, or it falls
    back to the default material outside the editor."""
    out = {}
    for slot, path in SLOT_MATERIALS.items():
        mi = unreal.load_asset(path)
        if mi is None:
            raise RuntimeError(f'missing {path}: run ue_reichstag_looks.py first')
        base = mi.get_base_material()
        if not base.get_editor_property('used_with_instanced_static_meshes'):
            unreal.MaterialEditingLibrary.set_material_usage(base, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
            base.modify()
            unreal.EditorLoadingAndSavingUtils.save_packages([base.get_outermost()], False)
            print('USAGE ISM set on', base.get_path_name())
        out[slot] = mi
    return out


def import_mesh(name):
    """Legacy FBX: Interchange drops UCX hulls and ignores FbxImportUI."""
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
    data.set_editor_property('generate_lightmap_u_vs', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = f'{MODEL}{name}.FBX'
    task.destination_path = MESH
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    ASSET_TOOLS.import_asset_tasks([task])


def finish_mesh(name, mats):
    path = f'{MESH}/{name}'
    mesh = unreal.load_asset(path)
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.set_editor_property('enabled', False)
    mesh.set_editor_property('nanite_settings', nanite)
    for i, slot in enumerate(mesh.get_editor_property('static_materials')):
        key = str(slot.get_editor_property('material_slot_name'))
        for slot_name, mi in mats.items():
            if key.startswith(slot_name):
                mesh.set_material(i, mi)
    save(path)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    print('MESH', name, 'tris', mesh.get_num_triangles(0), 'hulls', hulls,
          'slots', [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')])


def actor_blueprint():
    path = f'{ROOT}/{BP_NAME}'
    if ASSETS.does_asset_exist(path):
        return unreal.load_asset(path), path
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.Actor)
    bp = ASSET_TOOLS.create_asset(BP_NAME, ROOT, unreal.Blueprint, factory)
    return bp, path


def component(bp, cls, name):
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = SUBOBJECTS.k2_gather_subobject_data_for_blueprint(bp)
    for handle in handles:
        data = lib.get_data(handle)
        if str(lib.get_variable_name(data)) == name:
            return lib.get_object_for_blueprint(data, bp)
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=cls, blueprint_context=bp)
    handle, fail = SUBOBJECTS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError(f'add {name}: {fail}')
    SUBOBJECTS.rename_subobject(handle, unreal.Text(name))
    return lib.get_object_for_blueprint(lib.get_data(handle), bp)


def assemble():
    layout = json.loads(open(LAYOUT, encoding='utf-8').read())
    bp, path = actor_blueprint()
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    root = lib.get_object_for_blueprint(lib.get_data(SUBOBJECTS.k2_gather_subobject_data_for_blueprint(bp)[0]), bp)
    if isinstance(root, unreal.SceneComponent):
        root.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    by_piece = {}
    for row in layout['pieces']:
        by_piece.setdefault(row['piece'], []).append(row)
    total = 0
    for piece in PIECES:     # every piece, so one that left the layout loses its old instances
        rows = by_piece.get(piece, [])
        ism = component(bp, unreal.InstancedStaticMeshComponent, 'ISM_' + piece.replace('SM_RT_', ''))
        ism.set_editor_property('static_mesh', unreal.load_asset(f'{MESH}/{piece}'))
        ism.set_editor_property('mobility', unreal.ComponentMobility.STATIC)   # scenery: static lighting, cheaper
        ism.clear_instances()
        for r in rows:
            x, y, z = r['location_cm']
            t = unreal.Transform(location=unreal.Vector(x, y, z), rotation=unreal.Rotator(roll=0, pitch=0, yaw=r['yaw']))
            ism.add_instance(t)
        total += ism.get_instance_count()
        print('ISM', piece, ism.get_instance_count(), ism.get_editor_property('mobility'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    print('BP', path, 'instances', total, 'of', len(layout['pieces']))


def main(reimport=True, materials_only=False):
    mats = slot_materials()
    if materials_only:      # assign the slot materials to the existing meshes; no reimport, no Blueprint rebuild
        for name in PIECES:
            finish_mesh(name, mats)
        return
    if not reimport:
        assemble()
        return
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in PIECES:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    for name in PIECES:
        finish_mesh(name, mats)
    assemble()


main(reimport=globals().get('RT_REIMPORT', True), materials_only=globals().get('RT_MATERIALS_ONLY', False))
