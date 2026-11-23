"""Oder-Warthe Werk targets -> /Game/RTS_Survival/Blueprints/Environment/OderWarthe/Targets (headless only).

    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi -nosplash
        -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Imports the FBXs written by ow_targets_build.py (UCX hulls kept, simple-as-complex collision, Nanite off) and binds the
slots by name to existing materials: the OW kit instances, the faction concrete (a target instance under
MI_BF_SootedFortress_UVTile without the per-mesh curvature map), the faction camo net and the faction sandbag atlas.
Nothing outside the Targets folder is written. Log lines start with OWTGT; readback in Saved/OderWarthe/targets/import.json.
"""
import json

import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Targets/FBX/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Targets'
KIT_MATS = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Materials/'
BFS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/targets/import.json'
MESHES = ['SM_OW_TunnelPortal_Collapsed', 'SM_OW_Target_CommandPorch', 'SM_OW_Target_CommandPorch_Destroyed',
          'SM_OW_Target_Jammer', 'SM_OW_Target_Jammer_Destroyed', 'SM_OW_Target_RailGate',
          'SM_OW_Target_RailGate_Destroyed']
PORCH_CONCRETE = 'MI_OW_Target_PorchConcrete'

_EAL = unreal.EditorAssetLibrary
_MEL = unreal.MaterialEditingLibrary
_SUB = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REPORT = {}


def log(*a):
    line = ' '.join(str(x) for x in a)
    print('OWTGT ' + line)
    unreal.log_warning('OWTGT ' + line)


def save(asset, path):
    ok = _EAL.save_asset(path, only_if_is_dirty=False)
    log('saved' if ok else 'SAVE FAILED', path)
    return ok


def porch_concrete():
    path = ROOT + '/Materials/' + PORCH_CONCRETE
    parent = unreal.load_asset(BFS + 'MI_BF_SootedFortress_UVTile')
    mi = unreal.load_asset(path) if _EAL.does_asset_exist(path) else _TOOLS.create_asset(
        PORCH_CONCRETE, ROOT + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    _MEL.set_material_instance_parent(mi, parent)
    try:   # no baked curvature map for these meshes: the rebar gate falls back to the mesh-curvature path
        _MEL.set_material_instance_static_switch_parameter_value(mi, 'BF_UseCurvatureMap', False)
    except Exception as e:  # noqa: BLE001
        log('WARN switch', e)
    _MEL.update_material_instance(mi)
    save(mi, path)
    return mi


def material_table():
    mats = {}
    for n in ('Concrete', 'ConcreteDark', 'Earth', 'RustSteel', 'DarkSteel', 'Void', 'Wood'):
        mats['MI_OW_' + n] = unreal.load_asset(KIT_MATS + 'MI_OW_' + n)
    mats['MI_BF_SootedFortress_UVTile'] = porch_concrete()
    mats['M_BF_CamoNet'] = unreal.load_asset(BFS + 'Motion/MI_BF_CamoNet_Flutter')
    mats['M_GerSandBagScaffold_Atlas'] = unreal.load_asset(
        '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas')
    for k, v in mats.items():
        if v is None:
            raise RuntimeError('material missing: ' + k)
    return mats


def fbx_ui():
    ui = unreal.FbxImportUI()
    for k, v in dict(import_mesh=True, import_as_skeletal=False, import_materials=False, import_textures=False,
                     import_animations=False).items():
        ui.set_editor_property(k, v)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property('static_mesh_import_data')
    for k, v in dict(combine_meshes=True, auto_generate_collision=False, one_convex_hull_per_ucx=True,
                     generate_lightmap_u_vs=False, build_nanite=False, remove_degenerates=True).items():
        d.set_editor_property(k, v)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def import_mesh(name, mats):
    folder = ROOT + '/Meshes'
    task = unreal.AssetImportTask()
    task.filename = SRC + name + '.fbx'
    task.destination_path = folder
    task.destination_name = name
    task.automated, task.save, task.replace_existing, task.options = True, False, True, fbx_ui()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        _TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(folder + '/' + name)
    if mesh is None:
        raise RuntimeError('import failed ' + name)
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    statics = mesh.get_editor_property('static_materials')
    for i, s in enumerate(statics):
        sn = str(s.get_editor_property('material_slot_name'))
        key = next((k for k in sorted(mats, key=len, reverse=True) if sn.startswith(k)), None)
        if key is None:
            raise RuntimeError('%s: slot %s has no material' % (name, sn))
        mesh.set_material(i, mats[key])
    mesh.set_editor_property('light_map_coordinate_index', 1)
    body = mesh.get_editor_property('body_setup')
    if body is not None:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    save(mesh, folder + '/' + name)
    b = mesh.get_bounding_box()
    ag = body.get_editor_property('agg_geom') if body else None
    REPORT[name] = dict(
        bounds=[[round(b.min.x), round(b.min.y), round(b.min.z)], [round(b.max.x), round(b.max.y), round(b.max.z)]],
        tris=mesh.get_num_triangles(0), convex=len(ag.get_editor_property('convex_elems')) if ag else 0,
        materials=[mesh.get_material(i).get_name() for i in range(len(statics))])
    log('mesh', name, json.dumps(REPORT[name]))


def main():
    mats = material_table()
    for name in MESHES:
        import_mesh(name, mats)
    with open(OUT, 'w') as f:
        json.dump(REPORT, f, indent=1)
    log('DONE', len(REPORT))


main()
