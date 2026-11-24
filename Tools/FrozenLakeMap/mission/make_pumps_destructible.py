"""Make the Frozen Lake cryo pump Blueprints destructible (owner, 2026-11-23).

Reparents BP_FL_CryoPump_B_Intact and BP_FL_CryoPump_B_Pump3Dam from Actor to ADestructableEnvActor, like the
industrial oil tanks:
- construction script: SetupComponentCollisions on the pump mesh (weapon traces hit it);
- Health = 8 medium-tank shots (the Soviet research building's count; DeveloperSettings SovietBuildingShotHealth:
  221.4 damage per 75 mm shot, (8 - 0.5) x 221.4 = 1660);
- BP_OnUnitDies: gas explosion, two medium explosions, a scorch decal, then CollapseMeshWithSwapping to
  SM_FL_CryoPump_B_Destroyed. That broadcasts OnDestructibleCollapse, which Mission_FL_Bonus_DestroyPumps tracks
  (TrackDestructablesCollapse).
The flood that a dead pump should start is not built yet (UFrozenLakeIceSubsystem, FrozenLakeMission.md TODOs).
Usage: python make_pumps_destructible.py <port>
"""
import json
import sys

from uecp_client import action

KIT = '/Game/RTS_Survival/Blueprints/Environment/FrozenLake/CryoPump/'
PUMPS = [KIT + 'Blueprints/BP_FL_CryoPump_B_Intact', KIT + 'Blueprints/BP_FL_CryoPump_B_Pump3Dam']
DESTROYED_MESH = KIT + 'Meshes/SM_FL_CryoPump_B_Destroyed.SM_FL_CryoPump_B_Destroyed'
PUMP_HEALTH = 1660.0

CONSTRUCTION = [
    {'op': 'make_array', 'as': 'PumpMeshes', 'items': ['$Mesh_0']},
    {'op': 'call', 'func': 'SetupComponentCollisions',
     'args': {'MeshComponents': '$PumpMeshes', 'GeometryComponents': '$NoGeometryComponents',
              'bOverlapTanks': False}},
]

ON_DIES = [{'op': 'event', 'name': 'BP_OnUnitDies', 'body': [
    {'op': 'call', 'func': 'K2_GetActorLocation', 'as': 'PumpLocation'},
    {'op': 'make_struct', 'struct': 'Vector', 'as': 'StackOffset', 'fields': {'X': 0.0, 'Y': 0.0, 'Z': 600.0}},
    {'op': 'call', 'func': 'RTSBlueprintFunctionLibrary.RTSSpawnExplosionAtLocation',
     'args': {'WorldContextObject': '$self', 'ExplosionType': 'Gas', 'SpawnLocation': '=$PumpLocation + $StackOffset',
              'bPlaySound': True, 'Delay': 0.0}},
    {'op': 'call', 'func': 'RTSBlueprintFunctionLibrary.RTSSpawnExplosionAtLocation',
     'args': {'WorldContextObject': '$self', 'ExplosionType': 'Medium', 'SpawnLocation': '$PumpLocation',
              'bPlaySound': True, 'Delay': 1.0}},
    {'op': 'call', 'func': 'RTSBlueprintFunctionLibrary.RTSSpawnExplosionAtLocation',
     'args': {'WorldContextObject': '$self', 'ExplosionType': 'Medium', 'SpawnLocation': '=$PumpLocation + $StackOffset',
              'bPlaySound': True, 'Delay': 2.5}},
    {'op': 'make_struct', 'struct': 'Vector2D', 'as': 'ScorchSize', 'fields': {'X': 900.0, 'Y': 1300.0}},
    {'op': 'make_struct', 'struct': 'Vector2D', 'as': 'NoOffset', 'fields': {'X': 0.0, 'Y': 0.0}},
    {'op': 'call', 'func': 'RTSBlueprintFunctionLibrary.RTSSpawnDecal',
     'args': {'WorldContextObject': '$self', 'DecalType': 'Scorch', 'SpawnLocation': '$PumpLocation',
              'MinMaxSize': '$ScorchSize', 'LifeTime': 0.0, 'MinMaxXYOffset': '$NoOffset'}},
    {'op': 'make_struct', 'struct': 'SwapToDestroyedMesh', 'as': 'SwapToRuin', 'fields': {
        'ComponentToSwapOn': '$Mesh_0',
        'SwapSound': '/Game/RTS_Survival/StarterContent/Audio/Collapse02.Collapse02',
        'SwapFX': '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/'
                  'NS_DestructionSmoke.NS_DestructionSmoke',
        'FXOffset': {'op': 'make_struct', 'struct': 'Vector', 'fields': {'X': 0.0, 'Y': 0.0, 'Z': 50.0}},
        'MeshToSwapTo': DESTROYED_MESH,
        'Attenuation': '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse.Att_Collapse',
        'SoundConcurrency': '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse.SC_Collapse'}},
    {'op': 'call', 'func': 'CollapseMeshWithSwapping',
     'args': {'CollapseParameters': '$SwapToRuin', 'bNoLongerBlockWeaponsPostCollapse': True}},
]}]

PY_HEALTH = r'''
import unreal
path = "{path}"
bp = unreal.load_asset(path)
cls = unreal.load_class(None, path + "." + path.rsplit("/", 1)[1] + "_C")
cdo = unreal.get_default_object(cls)
cdo.set_editor_property("health", {health})
cdo.modify()
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
cdo = unreal.get_default_object(cls)
open(unreal.Paths.project_saved_dir() + "FrozenLake/pump_" + path.rsplit("/", 1)[1] + ".txt", "w").write(
    "saved=%s health=%s parent=%s" % (saved, cdo.get_editor_property("health"),
                                       unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_name()
                                       if hasattr(unreal.BlueprintEditorLibrary, "get_blueprint_parent_class") else "?"))
'''


def show(label, result):
    text = json.dumps(result)
    print(' ', label, text[:900] if ('error' in text.lower() or '"ok": false' in text) else 'ok', flush=True)


def main():
    port = int(sys.argv[1])
    for path in PUMPS:
        print('==', path.rsplit('/', 1)[1], flush=True)
        skel = action(port, 'get_blueprint_skeleton', {'blueprint_path': path})
        if not any(v['name'] == 'NoGeometryComponents' for v in skel.get('variables', [])):
            show('empty geometry array', action(port, 'add_variable', {'blueprint_path': path, 'items': [{
                'variable_name': 'NoGeometryComponents', 'variable_type': 'object',
                'sub_type': 'GeometryCollectionComponent', 'is_array': True, 'category': 'Destruction',
                'tooltip': 'Empty: the pump has no geometry collection (SetupComponentCollisions needs an array).'}]}))
        has_dies = any(e.get('name') == 'BP_OnUnitDies' for e in skel.get('events', []))
        if skel.get('parent_class') != 'DestructableEnvActor':
            show('reparent', action(port, 'reparent_blueprint', {
                'blueprint_path': path, 'new_parent_class': '/Script/RTS_Survival.DestructableEnvActor'}))
        show('construction', action(port, 'synthesize_graph', {
            'blueprint_path': path, 'function': 'UserConstructionScript', 'body': CONSTRUCTION, 'overwrite': True}))
        if not has_dies:
            show('on dies', action(port, 'synthesize_graph', {
                'blueprint_path': path, 'function': 'EventGraph', 'body': ON_DIES}))
        compiled = action(port, 'compile_blueprint', {'blueprint_path': path})
        print('  compile errors', compiled.get('error_count'), json.dumps(compiled)[:600] if compiled.get('error_count')
              else '', flush=True)
        show('health+save', action(port, 'execute_python', {
            'code': PY_HEALTH.format(path=path, health=PUMP_HEALTH),
            'description': 'Set cryo pump health and save'}))
        try:
            print(' ', open(r'D:\UE5Projects\RTS_Survival\Saved\FrozenLake\pump_' + path.rsplit('/', 1)[1]
                            + '.txt').read(), flush=True)
        except OSError as error:
            print('  no report', error)


if __name__ == '__main__':
    main()
