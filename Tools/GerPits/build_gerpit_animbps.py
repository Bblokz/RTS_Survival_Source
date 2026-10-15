"""Builds the German gun pits' animation assets through the editor bridge (UECP direct calls, see uecp.py).

  * AnimBp_GerPit_<Gun>: parent UGunPitAnimInstance. Deployed pose -> DefaultSlot -> elevation Modify Bone (pitch,
    bound to CradlePitchRotation / AimPitchAlpha) -> root Modify Bone (whole-gun yaw, TurntableYawRotation), both
    additive in component space, so the gun and the crew riding on its root bone traverse a full circle.
  * Montage_GerPit_<Gun>_Fire: the gun's fire animation in DefaultSlot (the crew component plays it on a shot).
  * Crew montages in DefaultSlot (AnimBp_B4Pit_Crew plays DefaultSlot): the team weapon loaders' own animations
    and the shell handler drills.

Run: python build_gerpit_animbps.py   (with scratchpad uecp.py importable and the bridge on 127.0.0.1:9855)
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
TW = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/'
LEFH = '/Game/RTS_Survival/Blueprints/Turrets/EmbeddedTurrets/LeFH_105mm/'
SQUAD_ANIMS = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/TeamWeaponAnims/'
GUNS = {
    'Pak40': {'skeleton': TW + 'Pak40_75MM/Mesh/Sk_FullAnimPak40TW_Skeleton',
              'pose': TW + 'Pak40_75MM/Animations/Deploy/Anim_TWPak40_Deployed',
              'fire': TW + 'Pak40_75MM/Animations/Weapon/Anim_TWPak40_Fire',
              'root': 'B_Base', 'elevation': 'B_Mantlet'},
    'LeFH18': {'skeleton': LEFH + 'Mesh/SK_FieldHaubitze_Skeleton',
               'pose': LEFH + 'Anims/Anim_LeFH_18_Base',
               'fire': LEFH + 'Anims/Anim_LeFH_18_Fire',
               'root': 'Artillery', 'elevation': 'B_Mantlet'},
    'SFH18': {'skeleton': TW + 'LFH18/Mesh/SK_Ger_SFH18Anim_Skeleton',
              'pose': TW + 'LFH18/PackAnims/Anim_SFH18_Deployed',
              'fire': TW + 'LFH18/WeaponAnim/Anim_SFH18_Fire',
              'root': '37mmFlak', 'elevation': 'Cannon_Elevate'},
}
CREW_MONTAGES = {
    'Montage_GerPit_LoadPak': SQUAD_ANIMS + 'Pak/AN_Infantry_LoadPAK',
    'Montage_GerPit_LoadHowitzer': SQUAD_ANIMS + 'Howitzer/AN_Infantry_LoadHowitzer',
    'Montage_GerPitPak_Handler_Reload': ROOT + '/Crew/Anim_GerPitPak_Handler_Reload',
    'Montage_GerPitHowitzer_Handler_Reload': ROOT + '/Crew/Anim_GerPitHowitzer_Handler_Reload',
}


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text)
    if not data.get('ok', True):
        print('FAILED', action, json.dumps(params)[:300], '->', text[:800])
        sys.exit(1)
    return data


def run_python(code):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': 'GerPit anim asset'}})
    return result.get('result', {}).get('content', [{}])[0].get('text', '')


def asset_exists(path):
    data = act('does_asset_exist', asset_path=path) if False else None
    return data


def create_anim_blueprint(gun, cfg):
    path = f'{ROOT}/Anims/AnimBp_GerPit_{gun}'
    code = f"""
import unreal
path = '{path}'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorAssetLibrary.delete_asset(path)
factory = unreal.AnimBlueprintFactory()
factory.set_editor_property('target_skeleton', unreal.load_asset('{cfg['skeleton']}'))
factory.set_editor_property('parent_class', unreal.GunPitAnimInstance)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AnimBp_GerPit_{gun}', '{ROOT}/Anims', unreal.AnimBlueprint, factory)
print('CREATED', asset.get_path_name() if asset else None)
"""
    print(run_python(code)[:400])
    return path


def remove_default_state_machine(abp):
    for node in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']:
        if node['node_class'] == 'AnimGraphNode_StateMachine':
            act('delete_anim_node', anim_blueprint_path=abp, node_guid=node['node_guid'])


def modify_bone(abp, bone, x, y, rotation_property, alpha_property=None):
    node = act('add_modify_bone', anim_blueprint_path=abp, position_x=x, position_y=y)['node_guid']
    for name, value in (('BoneToModify', bone), ('RotationMode', 'BMM_Additive'), ('RotationSpace', 'BCS_ComponentSpace'),
                        ('TranslationMode', 'BMM_Ignore'), ('ScaleMode', 'BMM_Ignore')):
        act('set_anim_node_property', anim_blueprint_path=abp, node_guid=node, property_name=name, property_value=value)
    act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Rotation', property_path=rotation_property)
    if alpha_property:
        act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Alpha', property_path=alpha_property)
    return node


def build_gun(gun, cfg):
    abp = create_anim_blueprint(gun, cfg)
    remove_default_state_machine(abp)
    seq = act('add_anim_node', anim_blueprint_path=abp, node_class='AnimGraphNode_SequencePlayer', position_x=-1400, position_y=0)['node_guid']
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='Sequence', property_value=cfg['pose'])
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='bLoopAnimation', property_value='true')
    slot = act('add_slot_node', anim_blueprint_path=abp, slot_name='DefaultSlot', position_x=-1150, position_y=0)['node_guid']
    to_cs = act('add_local_to_component_space', anim_blueprint_path=abp, position_x=-950, position_y=0)['node_guid']
    pitch = modify_bone(abp, cfg['elevation'], -700, 0, 'CradlePitchRotation', 'AimPitchAlpha')
    yaw = modify_bone(abp, cfg['root'], -400, 0, 'TurntableYawRotation')
    to_ls = act('add_component_to_local_space', anim_blueprint_path=abp, position_x=-150, position_y=0)['node_guid']
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=seq, target_node_guid=slot, target_pin='Source')
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=slot, target_node_guid=to_cs)
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=to_cs, target_node_guid=pitch, target_pin='ComponentPose')
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=pitch, target_node_guid=yaw, target_pin='ComponentPose')
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=yaw, target_node_guid=to_ls)
    act('wire_anim_node_to_output', anim_blueprint_path=abp, node_name='Component To Local')
    compiled = act('compile_blueprint', blueprint_path=abp)
    print(gun, 'compile', compiled.get('error_count'), compiled.get('warning_count'), compiled.get('errors'))
    act('save_asset', asset_path=abp)
    fire = act('create_montage', source_animation=cfg['fire'], name=f'Montage_GerPit_{gun}_Fire', save_path=f'{ROOT}/Anims')
    print(gun, 'fire montage', fire.get('asset_path'))


def build_crew_montages():
    for name, source in CREW_MONTAGES.items():
        result = act('create_montage', source_animation=source, name=name, save_path=f'{ROOT}/Crew')
        print('crew montage', result.get('asset_path'))


if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if which in ('all', 'guns'):
        for gun_name, gun_cfg in GUNS.items():
            build_gun(gun_name, gun_cfg)
    if which in ('all', 'crew'):
        build_crew_montages()
