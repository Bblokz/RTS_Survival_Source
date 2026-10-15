"""Builds the DShK pit's animation assets through the editor bridge (UECP direct calls, see uecp.py).

  AnimBp_DShKPit: parent UGunPitAnimInstance. Base pose -> DefaultSlot -> Elevation Modify Bone (pitch, bound to
    CradlePitchRotation / AimPitchAlpha) -> Traverse Modify Bone (yaw, TurntableYawRotation), both additive in
    component space; the tripod (Root) stays, the gun and the crew riding on Traverse turn a full circle.
  Montage_DShKPit_Fire / _Reload: the gun's recoil and box swap in DefaultSlot.
  Crew montages (DefaultSlot, AnimBp_B4Pit_Crew plays DefaultSlot): gunner fire / reload, loader reload.
The recoil and firing-shake montages blend in and out almost at once: they restart on every burst.

Run: python build_dshkpit_animbp.py   (with scratchpad uecp.py importable and the bridge on 127.0.0.1:9855)
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit'
SKELETON = ROOT + '/Meshes/SK_DShKPit_Gun_Skeleton'
GUN_MONTAGES = {'Montage_DShKPit_Fire': ROOT + '/Anims/Anim_DShKPit_Fire',
                'Montage_DShKPit_Reload': ROOT + '/Anims/Anim_DShKPit_Reload'}
CREW_MONTAGES = {'Montage_DShKPit_Gunner_Fire': ROOT + '/Crew/Anim_DShKPit_Gunner_Fire',
                 'Montage_DShKPit_Gunner_Reload': ROOT + '/Crew/Anim_DShKPit_Gunner_Reload',
                 'Montage_DShKPit_Loader_Reload': ROOT + '/Crew/Anim_DShKPit_Loader_Reload'}
QUICK_BLEND_MONTAGES = ('Montage_DShKPit_Fire', 'Montage_DShKPit_Gunner_Fire')
QUICK_BLEND_SECONDS = 0.04


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text, strict=False)
    if not data.get('ok', True):
        print('FAILED', action, json.dumps(params)[:300], '->', text[:800])
        sys.exit(1)
    return data


def run_python(code):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': 'DShK pit anim asset'}})
    return result.get('result', {}).get('content', [{}])[0].get('text', '')


def create_anim_blueprint():
    path = f'{ROOT}/Anims/AnimBp_DShKPit'
    print(run_python(f"""
import unreal
path = '{path}'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorAssetLibrary.delete_asset(path)
factory = unreal.AnimBlueprintFactory()
factory.set_editor_property('target_skeleton', unreal.load_asset('{SKELETON}'))
factory.set_editor_property('parent_class', unreal.GunPitAnimInstance)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AnimBp_DShKPit', '{ROOT}/Anims', unreal.AnimBlueprint, factory)
print('CREATED', asset.get_path_name() if asset else None)
""")[:300])
    return path


def modify_bone(abp, bone, x, rotation_property, alpha_property=None):
    node = act('add_modify_bone', anim_blueprint_path=abp, position_x=x, position_y=0)['node_guid']
    for name, value in (('BoneToModify', bone), ('RotationMode', 'BMM_Additive'), ('RotationSpace', 'BCS_ComponentSpace'),
                        ('TranslationMode', 'BMM_Ignore'), ('ScaleMode', 'BMM_Ignore')):
        act('set_anim_node_property', anim_blueprint_path=abp, node_guid=node, property_name=name, property_value=value)
    act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Rotation', property_path=rotation_property)
    if alpha_property:
        act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Alpha', property_path=alpha_property)
    return node


def build_anim_blueprint():
    abp = create_anim_blueprint()
    for node in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']:
        if node['node_class'] == 'AnimGraphNode_StateMachine':
            act('delete_anim_node', anim_blueprint_path=abp, node_guid=node['node_guid'])
    seq = act('add_anim_node', anim_blueprint_path=abp, node_class='AnimGraphNode_SequencePlayer', position_x=-1400, position_y=0)['node_guid']
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='Sequence',
        property_value=ROOT + '/Anims/Anim_DShKPit_Base')
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='bLoopAnimation', property_value='true')
    slot = act('add_slot_node', anim_blueprint_path=abp, slot_name='DefaultSlot', position_x=-1150, position_y=0)['node_guid']
    to_cs = act('add_local_to_component_space', anim_blueprint_path=abp, position_x=-950, position_y=0)['node_guid']
    pitch = modify_bone(abp, 'Elevation', -700, 'CradlePitchRotation', 'AimPitchAlpha')
    yaw = modify_bone(abp, 'Traverse', -400, 'TurntableYawRotation')
    act('add_component_to_local_space', anim_blueprint_path=abp, position_x=-150, position_y=0)
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=seq, target_node_guid=slot, target_pin='Source')
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=slot, target_node_guid=to_cs)
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=to_cs, target_node_guid=pitch, target_pin='ComponentPose')
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=pitch, target_node_guid=yaw, target_pin='ComponentPose')
    to_ls = [n for n in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']
             if n['node_class'] == 'AnimGraphNode_ComponentToLocalSpace'][0]['node_guid']
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=yaw, target_node_guid=to_ls)
    act('wire_anim_node_to_output', anim_blueprint_path=abp, node_name='Component To Local')
    compiled = act('compile_blueprint', blueprint_path=abp)
    print('AnimBp_DShKPit compile', compiled.get('error_count'), compiled.get('warning_count'), compiled.get('errors'))
    act('save_asset', asset_path=abp)


def build_montages():
    for folder, montages in (('Anims', GUN_MONTAGES), ('Crew', CREW_MONTAGES)):
        for name, source in montages.items():
            result = act('create_montage', source_animation=source, name=name, save_path=f'{ROOT}/{folder}')
            print('montage', result.get('asset_path'))
    for name in QUICK_BLEND_MONTAGES:
        folder = 'Anims' if name in GUN_MONTAGES else 'Crew'
        print(run_python(f"""
import unreal
path = '{ROOT}/{folder}/{name}'
montage = unreal.load_asset(path)
for prop in ('blend_in', 'blend_out'):
    blend = montage.get_editor_property(prop)
    blend.set_editor_property('blend_time', {QUICK_BLEND_SECONDS})
    montage.set_editor_property(prop, blend)
unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
print('BLEND', path, montage.get_editor_property('blend_in').get_editor_property('blend_time'))
""")[:300])


if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if which in ('all', 'animbp'):
        build_anim_blueprint()
    if which in ('all', 'montages'):
        build_montages()
