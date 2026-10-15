"""Builds the BS-3 team weapon's animation blueprint, aim offset and montages through the editor bridge.

The BS-3 shares the ZiS-3 skeleton, so its anim blueprint is a copy of the ZiS-3's (aim offset on the deployed
pose, DefaultSlot for the pack / deploy and wheel montages, Weapon slot for the recoil, layered per bone) with every
ZiS-3 asset swapped for the BS-3's:
  AO_BS3                         a copy of AO_Zis3 with the BS-3's aim poses at the same sample points
  Montage_BS3_DeployedToPacked   DefaultSlot (played backwards to deploy)
  Montage_BS3_WheelsFWD          DefaultSlot (looped by the anim instance while the gun is pushed)
  Montage_BS3_Fire               Weapon slot
  AnimBp_TW_BS3                  class defaults Sequences / GunMontages / DeployAndMvtMontages point at the above

Run: python build_bs3_animbp.py   (scratchpad uecp.py importable; UECP_PORT picks an editor other than the 9855 broker's)
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/BS3_100MM'
ZIS = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/Zis_76MM'
ANIM_BP = ROOT + '/AnimBp_TW_BS3'
AO = ROOT + '/WeaponAnims/AO_BS3'
MONTAGES = {  # name: (source animation, folder, slot)
    'Montage_BS3_DeployedToPacked': (ROOT + '/DeployPacked/Anim_BS3_DeployedToPacked', 'DeployPacked', 'DefaultSlot'),
    'Montage_BS3_WheelsFWD': (ROOT + '/WheelsAnims/Anim_BS3_WheelsFWD', 'WheelsAnims', 'DefaultSlot'),
    'Montage_BS3_Fire': (ROOT + '/WeaponAnims/Anim_BS3_Fire', 'WeaponAnims', 'Weapon'),
}


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text, strict=False)
    if not data.get('ok', True) or data.get('success') is False:
        print('FAILED', action, json.dumps(params)[:300], '->', text[:800])
        sys.exit(1)
    return data


def run_python(code, description):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': description}})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '')
    print(text[:600])
    return text


def asset_ref(kind, path):
    return f"/Script/Engine.{kind}'{path}.{path.split('/')[-1]}'"


def build_montages():
    for name, (source, folder, slot) in MONTAGES.items():
        act('create_montage', source_animation=source, name=name, save_path=f'{ROOT}/{folder}')
        if slot != 'DefaultSlot':
            act('set_montage_slot_name', montage_path=f'{ROOT}/{folder}/{name}', new_slot_name=slot)
        act('save_asset', asset_path=f'{ROOT}/{folder}/{name}')
        print('montage', name, slot)


# Aim offset samples (yaw, pitch) of AO_Zis3 and the BS-3 pose that takes each place.
AIM_SAMPLES = {(0, 30): 'Up30', (0, 0): 'Base', (0, -10): 'Down10', (-25, 0): 'Left25', (25, 0): 'Right25',
               (25, -10): 'Right25Down10', (25, 30): 'Right25Up30', (-25, 30): 'Left25Up30', (-25, -10): 'Left25Down10'}


def build_aim_offset():
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{AO}'):
    eal.delete_asset('{AO}')
eal.duplicate_asset('{ZIS}/WheelsAnims/AO_Zis3', '{AO}')
eal.save_asset('{AO}', only_if_is_dirty=False)
""", 'BS3 aim offset duplicate')
    # Python sees the samples only as copies; the bridge edits them in place.
    for (yaw, pitch), pose in AIM_SAMPLES.items():
        act('set_blendspace_sample_animation', blendspace_path=AO, sample_x=yaw, sample_y=pitch,
            new_animation_path=f'{ROOT}/WeaponAnims/Anim_BS3_{pose}')
    act('save_asset', asset_path=AO)
    run_python(f"""
import unreal
ao = unreal.load_asset('{AO}')
print('AO', sorted((s.get_editor_property('animation').get_name(), s.get_editor_property('sample_value').x,
                    s.get_editor_property('sample_value').y) for s in ao.get_editor_property('sample_data')))
""", 'BS3 aim offset check')


def build_anim_blueprint():
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{ANIM_BP}'):
    eal.delete_asset('{ANIM_BP}')
print('DUP', eal.duplicate_asset('{ZIS}/SK_TW_Zis3_Skeleton_AnimBlueprint', '{ANIM_BP}'))
eal.save_asset('{ANIM_BP}', only_if_is_dirty=False)
""", 'BS3 anim blueprint duplicate')
    for node in act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']:
        asset = node.get('current_asset', '')
        if node['node_class'] == 'AnimGraphNode_RotationOffsetBlendSpace':
            act('set_anim_node_property', anim_blueprint_path=ANIM_BP, node_guid=node['node_guid'], property_name='BlendSpace',
                property_value=AO)
        elif node['node_class'] == 'AnimGraphNode_SequencePlayer' and 'Anim_Zis3_' in asset:
            twin = asset.split('/')[-1].split('.')[0].replace('Anim_Zis3_', 'Anim_BS3_')
            act('set_anim_node_property', anim_blueprint_path=ANIM_BP, node_guid=node['node_guid'], property_name='Sequence',
                property_value=f'{ROOT}/DeployPacked/{twin}')
    act('compile_blueprint', blueprint_path=ANIM_BP)
    run_python(f"""
import unreal
bp = unreal.load_asset('{ANIM_BP}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
sequences = cdo.get_editor_property('Sequences')
sequences.set_editor_property('packed_sequence', unreal.load_asset('{ROOT}/DeployPacked/Anim_BS3_Packed'))
sequences.set_editor_property('deployed_sequence', unreal.load_asset('{ROOT}/DeployPacked/Anim_BS3_Deployed'))
cdo.set_editor_property('Sequences', sequences)
gun = cdo.get_editor_property('GunMontages')
fire = unreal.load_asset('{ROOT}/WeaponAnims/Montage_BS3_Fire')
gun.set_editor_property('fire_montage', fire)
gun.set_editor_property('burst_fire_montage', fire)
cdo.set_editor_property('GunMontages', gun)
moves = cdo.get_editor_property('DeployAndMvtMontages')
moves.set_editor_property('deploy_to_packed', unreal.load_asset('{ROOT}/DeployPacked/Montage_BS3_DeployedToPacked'))
moves.set_editor_property('move_fwd_montage', unreal.load_asset('{ROOT}/WheelsAnims/Montage_BS3_WheelsFWD'))
cdo.set_editor_property('DeployAndMvtMontages', moves)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
print('CDO', cdo.get_editor_property('Sequences'), cdo.get_editor_property('GunMontages'), cdo.get_editor_property('DeployAndMvtMontages'))
""", 'BS3 anim blueprint defaults')
    nodes = act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']
    print('assets in graph', sorted({n.get('current_asset', '').split('/')[-1] for n in nodes if n.get('current_asset')}))
    compiled = act('compile_blueprint', blueprint_path=ANIM_BP)
    print('AnimBp_TW_BS3 compile', compiled.get('error_count'), compiled.get('warning_count'), compiled.get('errors'))
    act('save_asset', asset_path=ANIM_BP)


if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if which in ('all', 'montages'):
        build_montages()
    if which in ('all', 'ao'):
        build_aim_offset()
    if which in ('all', 'animbp'):
        build_anim_blueprint()
