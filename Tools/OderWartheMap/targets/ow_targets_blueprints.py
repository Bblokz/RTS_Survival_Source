"""Oder-Warthe Werk targets: the destructible Blueprints (headless only, after ow_targets_import.py).

    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi -nosplash
        -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Each target is a duplicate of BP_Industrial_Dest_Container (parent BP_DestructableEnvActor -> ADestructableEnvActor):
  construction  SetupComponentCollisions(StaticMesh); SetRandomStaticMesh over five entries, all set to the intact mesh;
                Health from the row of the construction script's data table, here
                DT_OW_WerkTargets (FRTS_DestData rows OW_*) next to the Blueprints, so no shared table is touched.
  death         BP_OnUnitDies -> CollapseMeshWithSwapping(StaticMesh -> destroyed mesh, NS_DestructionSmoke, a collapse
                cue, bNoLongerBlockWeaponsPostCollapse): the wreck stays in the world and no longer blocks shells.
The graph literals are retargeted with the project's console command RTS.Blueprint.SetPinDefaults (Python cannot reach
pin defaults). Heavy-only damage is ADestructableEnvActor's flat DamageReduction per hit.
"""
import json

import unreal

ROOT = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Targets'
TEMPLATE = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Industrial/BP_Industrial_Dest_Container'
MESHES = ROOT + '/Meshes/'
SMOKE = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke.NS_DestructionSmoke'
COLLAPSE = '/Game/RTS_Survival/StarterContent/Audio/Collapse02.Collapse02'
RUBBLE = '/Game/RTS_Survival/Sounds/Explosions/General_Rubble/Cue_Explosion_Rubble_Small.Cue_Explosion_Rubble_Small'
DT_PATH = ROOT + '/Blueprints/DT_OW_WerkTargets'          # own rows (FRTS_DestData) so no shared table changes
DT_STRUCT = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/DestructableData/FRTS_DestData'
PIN_JSON = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/targets/pin_edits.json'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/targets/blueprints.json'

# Aim points are in the actor frame (Unreal: x forward, y right, z up), on the parts a shell should strike.
TARGETS = {
    'BP_OW_TunnelMouth': dict(
        row='OW_TunnelMouth',
        intact='/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Meshes/SM_OW_TunnelPortal',
        destroyed=MESHES + 'SM_OW_TunnelPortal_Collapsed', sound=COLLAPSE, fx_offset='300.000000,0.000000,450.000000',
        health=2000.0, reduction=150.0, keep_nav=False, block_after=True,
        aim=[(300, 0, 620), (300, -560, 400), (300, 560, 400), (150, 0, 760)]),
    'BP_OW_Target_CommandPorch': dict(
        row='OW_CommandPorch',
        intact=MESHES + 'SM_OW_Target_CommandPorch', destroyed=MESHES + 'SM_OW_Target_CommandPorch_Destroyed',
        sound=RUBBLE, fx_offset='80.000000,0.000000,200.000000', health=1500.0, reduction=20.0, keep_nav=True,
        aim=[(80, 0, 200), (60, -200, 150), (100, 220, 220)]),
    'BP_OW_Target_Jammer': dict(
        row='OW_Jammer',
        intact=MESHES + 'SM_OW_Target_Jammer', destroyed=MESHES + 'SM_OW_Target_Jammer_Destroyed',
        sound=RUBBLE, fx_offset='0.000000,330.000000,120.000000', health=900.0, reduction=0.0, keep_nav=True,
        aim=[(0, 330, 90), (60, 260, 140), (-60, 380, 90), (0, 0, 1135)]),
    'BP_OW_Target_RailGate': dict(
        row='OW_RailGate',
        intact=MESHES + 'SM_OW_Target_RailGate', destroyed=MESHES + 'SM_OW_Target_RailGate_Destroyed',
        sound=COLLAPSE, fx_offset='60.000000,0.000000,220.000000', health=1500.0, reduction=20.0, keep_nav=True,
        aim=[(30, 0, 220), (30, -250, 220), (30, 250, 220)]),
}

_EAL = unreal.EditorAssetLibrary
_SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
_SDL = unreal.SubobjectDataBlueprintFunctionLibrary
REPORT = {}


def log(*a):
    line = ' '.join(str(x) for x in a)
    print('OWTGT ' + line)
    unreal.log_warning('OWTGT ' + line)


def subobjects(bp):
    out = {}
    for h in _SDS.k2_gather_subobject_data_for_blueprint(bp):
        obj = _SDL.get_object(_SDL.get_data(h))
        if obj is not None:
            out[obj.get_name().replace('_GEN_VARIABLE', '')] = (h, obj)
    return out


def duplicate(name, cfg):
    path = ROOT + '/Blueprints/' + name
    if _EAL.does_asset_exist(path):
        _EAL.delete_asset(path)
    bp = _EAL.duplicate_asset(TEMPLATE, path)
    subs = subobjects(bp)
    mesh = unreal.load_asset(cfg['intact'])
    subs['StaticMesh'][1].set_editor_property('static_mesh', mesh)
    if not cfg['keep_nav'] and 'RTSNavCollisionEnvObstacle' in subs:
        # The obstacle is inherited (cannot be deleted); a default area makes it a no-op so the open mouth stays
        # walkable for the waves. The static navmesh bakes the portal itself.
        nav = _SDL.get_object_for_blueprint(_SDL.get_data(subs['RTSNavCollisionEnvObstacle'][0]), bp)
        nav.set_editor_property('area_class', unreal.NavArea_Default)
        log(name, 'nav obstacle area', nav.get_editor_property('area_class'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    _EAL.save_asset(path, only_if_is_dirty=False)
    return path


def pin_edits(path, cfg):
    d = cfg['destroyed']
    mesh = d + '.' + d.split('/')[-1]
    return dict(blueprint=path, edits=[
        dict(graph='EventGraph', node='K2Node_MakeStruct_3', pin='MeshToSwapTo', value=mesh),
        dict(graph='EventGraph', node='K2Node_MakeStruct_3', pin='SwapFX', value=SMOKE),
        dict(graph='EventGraph', node='K2Node_MakeStruct_3', pin='SwapSound', value=cfg['sound']),
        dict(graph='EventGraph', node='K2Node_MakeStruct_3', pin='FXOffset', value=cfg['fx_offset']),
        # the collapsed tunnel is a 22 m hill of earth and concrete: it keeps stopping shells (the others let them pass)
        dict(graph='EventGraph', node='K2Node_CallFunction_2', pin='bNoLongerBlockWeaponsPostCollapse',
             value='false' if cfg.get('block_after') else 'true'),
        dict(graph='UserConstructionScript', node='K2Node_GetDataTableRow_0', pin='DataTable',
             value=DT_PATH + '.' + DT_PATH.split('/')[-1]),
        dict(graph='UserConstructionScript', node='K2Node_GetDataTableRow_0', pin='RowName', value=cfg['row']),
    ] + [  # the template picks a random container for StaticMesh once the row is found: all five are the intact mesh
        dict(graph='UserConstructionScript', node='K2Node_MakeStruct_%d' % k, pin='Mesh',
             value=cfg['intact'] + '.' + cfg['intact'].split('/')[-1]) for k in range(5)])


def data_table():
    """Health per target lives in the construction script's table row (the template reads Health from it)."""
    if not _EAL.does_asset_exist(DT_PATH):
        f = unreal.DataTableFactory()
        f.set_editor_property('struct', unreal.load_asset(DT_STRUCT))
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(DT_PATH.split('/')[-1], ROOT + '/Blueprints',
                                                               unreal.DataTable, f)
    dt = unreal.load_asset(DT_PATH)
    csv = '---,Health\n' + ''.join('%s,"%f"\n' % (c['row'], c['health']) for c in TARGETS.values())
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(dt, csv)
    rows = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)]
    _EAL.save_asset(DT_PATH, only_if_is_dirty=False)
    log('data table', ok, rows)


def class_defaults(path, cfg):
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('health', cfg['health'])
    cdo.set_editor_property('damage_reduction', cfg['reduction'])
    cdo.set_editor_property('aim_offset_points', [unreal.Vector(*p) for p in cfg['aim']])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log('compiled', path)    # compiler errors, if any, follow this line in the log
    ok = _EAL.save_asset(path, only_if_is_dirty=False)
    cdo = unreal.get_default_object(bp.generated_class())
    subs = subobjects(bp)
    REPORT[path] = dict(
        saved=ok, health=cdo.get_editor_property('health'), damage_reduction=cdo.get_editor_property('damage_reduction'),
        aim=[[p.x, p.y, p.z] for p in cdo.get_editor_property('aim_offset_points')],
        mesh=subs['StaticMesh'][1].get_editor_property('static_mesh').get_path_name(),
        nav_area=str(_SDL.get_object_for_blueprint(_SDL.get_data(subs['RTSNavCollisionEnvObstacle'][0]), bp)
                     .get_editor_property('area_class')).split("'")[1],
        components=sorted(subs))
    log('bp', path, json.dumps(REPORT[path]))


def main():
    data_table()
    paths = {name: duplicate(name, cfg) for name, cfg in TARGETS.items()}
    with open(PIN_JSON, 'w') as f:
        json.dump([pin_edits(paths[n], c) for n, c in TARGETS.items()], f, indent=1)
    unreal.SystemLibrary.execute_console_command(None, 'RTS.Blueprint.SetPinDefaults ' + PIN_JSON)
    for name, cfg in TARGETS.items():
        class_defaults(paths[name], cfg)
    with open(OUT, 'w') as f:
        json.dump(REPORT, f, indent=1)
    log('DONE', len(REPORT))


main()
