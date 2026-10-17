"""Builds the AnimGraph of AnimBp_EmbInfantryTurret (run once on a fresh anim blueprint, parent
UEmbeddedInfantryTurretAnimInstance, skeleton InfantryMaster_Skeleton):
Idle_Rifle_Ironsights -> Slot 'FIreSlot' (the infantry fire montages' slot) -> AimOffset Player AO_Rifle
(X bound to Yaw, Y bound to Pitch) -> Output Pose.
Run: python build_anim_bp.py   (uecp.py importable via PYTHONPATH)
"""
import json, sys
from uecp import call_tool
ABP = '/Game/RTS_Survival/Blueprints/Turrets/EmbInfantryTurrets/AnimBp_EmbInfantryTurret'
def act(action, **params):
    r = call_tool('call_action', {'action': action, 'params': params})
    text = r.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text, strict=False)
    if not data.get('ok', True) or data.get('success') is False:
        print('FAILED', action, params, text[:1500]); sys.exit(1)
    return data
seq = act('add_anim_node', anim_blueprint_path=ABP, node_class='AnimGraphNode_SequencePlayer', position_x=-700, position_y=0)
print('seq', json.dumps(seq)[:600])
seq_g = seq['node_guid']
act('set_anim_node_property', anim_blueprint_path=ABP, node_guid=seq_g, property_name='Sequence',
    value='/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimLibrary/StarterAimingAnimations/Idle_Rifle_Ironsights.Idle_Rifle_Ironsights')
slot = act('add_slot_node', anim_blueprint_path=ABP, slot_name='FIreSlot', position_x=-450, position_y=0)['node_guid']
ao = act('add_aim_offset_player', anim_blueprint_path=ABP, position_x=-200, position_y=0,
         blendspace_path='/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/AO_Rifle_Standing/AO_Rifle')
print('ao', json.dumps(ao)[:800])
ao_g = ao['node_guid']
act('connect_anim_nodes', anim_blueprint_path=ABP, source_node_guid=seq_g, target_node_guid=slot)
act('connect_anim_nodes', anim_blueprint_path=ABP, source_node_guid=slot, target_node_guid=ao_g, target_pin='BasePose')
act('bind_anim_pin', anim_blueprint_path=ABP, node_guid=ao_g, pin='X', property_path='Yaw')
act('bind_anim_pin', anim_blueprint_path=ABP, node_guid=ao_g, pin='Y', property_path='Pitch')
print(json.dumps(act('wire_anim_node_to_output', anim_blueprint_path=ABP, node_name=ao['node_title'] if 'node_title' in ao else 'AimOffset Player'))[:400])
