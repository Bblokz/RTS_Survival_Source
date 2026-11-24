"""Throwaway-editor run: (re)build the gate VAT material chain and reimport the leaf meshes, then quit.

UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
  -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
  -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/RusWallGates/gate_fix_headless.py"
  -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Steps: ue_gate_mf.py, ue_patch_gate_masters.py, leaf mesh reimport (ue_import_gates.py meshes only, A/C leaf),
ue_gate_collision.py, ue_gate_materials.py. Waits FIXED_WAIT seconds for shader compiles (fills the DDC) and quits.
Log lines start with GATEFIX.
"""
import unreal

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/RusWallGates/'
BUILD = 'D:/UE5Projects/RTS_Survival/Saved/RusWallGates/build'
FIXED_WAIT = 300.0
S = {'t': 0.0, 'phase': 'wait', 'in': False}


def run(name, extra=None):
    ns = {'__name__': '__gatefix__', 'BUILD': BUILD}
    ns.update(extra or {})
    exec(compile(open(TOOLS + name, encoding='utf-8').read(), name, 'exec'), ns)
    unreal.log('GATEFIX ran ' + name)


def reimport_leaves():
    SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    EAL = unreal.EditorAssetLibrary
    folder = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/Meshes'
    tasks = []
    for k in ('A', 'C'):
        t = unreal.AssetImportTask()
        t.filename = BUILD + '/SM_RusWallGate%s_Leaf.fbx' % k
        t.destination_path = folder
        t.destination_name = 'SM_RusWallGate%s_Leaf' % k
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for ad in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, recursive=False):
        if str(ad.asset_class_path.asset_name) in ('Material', 'MaterialInstanceConstant'):
            unreal.log('GATEFIX deleting imported %s %s' % (ad.package_name, EAL.delete_asset(str(ad.package_name))))
    for k in ('A', 'C'):
        mesh = unreal.load_asset('%s/SM_RusWallGate%s_Leaf' % (folder, k))
        nanite = mesh.get_editor_property('nanite_settings')
        nanite.enabled = False
        mesh.set_editor_property('nanite_settings', nanite)
        build = SMS.get_lod_build_settings(mesh, 0)
        build.use_full_precision_u_vs = True
        build.generate_lightmap_u_vs = False
        build.recompute_normals = False
        build.recompute_tangents = True
        SMS.set_lod_build_settings(mesh, 0, build)
        mesh.set_editor_property('light_map_coordinate_index', 1)
        unreal.log('GATEFIX leaf %s uv=%d slots=%s saved=%s' % (
            k, SMS.get_num_uv_channels(mesh, 0),
            [str(s.material_slot_name) for s in mesh.get_editor_property('static_materials')],
            EAL.save_loaded_asset(mesh, False)))


def describe():
    L = unreal.NomadicDeployEditorLibrary
    R = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/'
    for n in ('Materials/MI_RusWallGateA_Leaf_GateSteel', 'Materials/MI_RusWallGateC_Leaf_Concrete',
              'Holes/MI_RusWallGateA_Leaf_GateSteel_WallHoles', 'Holes/MI_RusWallGateC_Leaf_Concrete_WallHoles'):
        unreal.log('GATEFIX %s %s' % (n, L.describe_compiled_resource(unreal.load_asset(R + n))))


def tick(delta):
    S['t'] += delta
    if S['in']:
        return
    S['in'] = True
    try:
        if S['phase'] == 'wait' and S['t'] > 15.0:
            S['phase'] = 'fixing'
            run('ue_gate_mf.py')
            run('ue_patch_gate_masters.py')
            reimport_leaves()
            run('ue_gate_collision.py')
            run('ue_gate_materials.py')
            S['phase'], S['mark'] = 'compile', S['t']
        elif S['phase'] == 'compile' and S['t'] - S['mark'] > FIXED_WAIT:
            describe()
            S['phase'] = 'done'
            unreal.SystemLibrary.quit_editor()
    except Exception as error:  # noqa: BLE001
        unreal.log_error('GATEFIX ERROR %r' % error)
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()
    finally:
        S['in'] = False


unreal.register_slate_post_tick_callback(tick)
unreal.log('GATEFIX loaded')
