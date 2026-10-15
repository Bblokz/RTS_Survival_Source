"""Gives every gun pit a gun that bucks on a shot and a crew that braces, through the editor bridge (see uecp.py).

Gun (UGunPitAnimInstance::PlayFireKick). Each pit gun anim blueprint gets Modify Bone nodes on its kick bones,
between the elevation node and the root yaw node: additive component-space rotation FireKickRotation and
translation FireKickTranslation. The kick bones carry the whole gun but not the root bone, whose pit-origin socket
holds the crew, crate and rounds, so only the gun moves. The class defaults' FireKick settings tune it per gun.

Crew (UGunPitCrewAnimInstance::PlayFireFlinch). AnimBp_B4Pit_Crew, the crew anim blueprint of every pit, gets three
additive component-space Modify Bone nodes after its DefaultSlot (spine_01, spine_03, head). The brace layers over
whatever the man is doing, idle or mid-drill, so it never cuts the loading drill or the round in his hands.
The DShK pit fires in bursts: its crew component gets CrewFireFlinchStrength 0.

Re-running rebuilds the nodes (existing kick and flinch nodes are deleted first).
Run: python build_fire_feedback.py [crew,guns,dshk,flak] [Flak37,BS3Pit,...]   (scratchpad uecp.py importable; UECP_PORT picks an editor)
"""
import json
import sys
from uecp import call_tool

CREW_ANIM_BP = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/AnimBp_B4Pit_Crew'
FLINCH_BONES = [('spine_01', 'FlinchLowerSpineRotation'), ('spine_03', 'FlinchUpperSpineRotation'),
                ('head', 'FlinchHeadRotation')]
DSHK_TURRET = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit/Turrets/RTS_BP_EmbTurret_RusDShKPit'
FLAK_TURRET = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Turrets/RTS_BP_EmbTurret_GerPit_Flak37'
# The Flak crew brace through each burst at half strength.
FLAK_CREW_FLINCH_STRENGTH = 0.5
GER = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Anims/'
# Per gun: kick bones (the first is the one the rock pivot is measured from), gun frame socket and the kick.
GUNS = {
    GER + 'AnimBp_GerPit_Pak40': {
        # The Pak's parts hang straight off its root bone: they shove back together, without a rock.
        'bones': ['B_Mantlet', 'B_LeftWheel', 'B_RightWheel', 'B_LeftLeg', 'B_RightLeg'], 'socket': 'PitOrigin',
        'jolt': 6.0, 'rock': 0.0, 'pivot': 200.0, 'muzzle': 2.0, 'hz': 5.0, 'damping': 0.35},
    GER + 'AnimBp_GerPit_LeFH18': {
        'bones': ['B_Base'], 'socket': 'PitOrigin', 'jolt': 5.0, 'rock': 0.8, 'pivot': 260.0, 'muzzle': 2.0,
        'hz': 4.0, 'damping': 0.35},
    GER + 'AnimBp_GerPit_SFH18': {
        'bones': ['ROOT'], 'socket': 'PitOrigin', 'jolt': 6.0, 'rock': 0.9, 'pivot': 320.0, 'muzzle': 2.5,
        'hz': 3.5, 'damping': 0.35},
    # The Flak 37 fires 6-round bursts: a light, quick kick per shot; the kicks of a burst add up.
    GER + 'AnimBp_GerPit_Flak37': {
        'bones': ['B_Root'], 'socket': 'PitOrigin', 'jolt': 2.2, 'rock': 0.3, 'pivot': 150.0, 'muzzle': 0.8,
        'hz': 9.0, 'damping': 0.4},
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Anims/AnimBp_53KPit': {
        'bones': ['Wheels'], 'socket': 'PitOrigin53K', 'jolt': 5.0, 'rock': 1.0, 'pivot': 220.0, 'muzzle': 2.0,
        'hz': 5.5, 'damping': 0.35},
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Anims/AnimBp_BS3Pit': {
        'bones': ['Wheels'], 'socket': 'PitOriginBS3', 'jolt': 7.0, 'rock': 1.0, 'pivot': 300.0, 'muzzle': 2.5,
        'hz': 4.0, 'damping': 0.35},
    # The B-4 and ML-20: only the top carriage and gun buck; the crane, cart and rack on B_Root stay.
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/AnimBp_B4Pit_Gun': {
        'bones': ['B_Turret'], 'socket': 'PitOrigin', 'jolt': 3.0, 'rock': 0.4, 'pivot': 60.0, 'muzzle': 1.5,
        'hz': 3.0, 'damping': 0.4},
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/AnimBp_ML20Pit_Gun': {
        'bones': ['B_Turret'], 'socket': 'PitOrigin', 'jolt': 4.0, 'rock': 0.6, 'pivot': 60.0, 'muzzle': 2.0,
        'hz': 3.5, 'damping': 0.38},
}
KICK_NODE_SPACING = 160


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
    print(text[:700])
    return text


def nodes_of(abp):
    return act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']


def additive_modify_bone(abp, bone, x, y, rotation_property, translation_property=None):
    node = act('add_modify_bone', anim_blueprint_path=abp, position_x=x, position_y=y)['node_guid']
    settings = [('BoneToModify', bone), ('RotationMode', 'BMM_Additive'), ('RotationSpace', 'BCS_ComponentSpace'),
                ('ScaleMode', 'BMM_Ignore')]
    settings += [('TranslationMode', 'BMM_Additive'), ('TranslationSpace', 'BCS_ComponentSpace')] if translation_property \
        else [('TranslationMode', 'BMM_Ignore')]
    for name, value in settings:
        act('set_anim_node_property', anim_blueprint_path=abp, node_guid=node, property_name=name, property_value=value)
    act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Rotation', property_path=rotation_property)
    if translation_property:
        act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Translation', property_path=translation_property)
    return node


def compile_and_save(abp):
    compiled = act('compile_blueprint', blueprint_path=abp)
    print(abp.split('/')[-1], 'compile errors', compiled.get('error_count'), compiled.get('errors'))
    # A package another editor holds open cannot be written; report it and carry on with the rest.
    result = call_tool('call_action', {'action': 'save_asset', 'params': {'asset_path': abp}})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    print(abp.split('/')[-1], 'saved', json.loads(text, strict=False).get('ok', True))


# ---------------------------------------------------------------------------------------------------------------
# Crew
# ---------------------------------------------------------------------------------------------------------------
def build_crew():
    abp = CREW_ANIM_BP
    for node in nodes_of(abp):
        if node['node_class'] in ('AnimGraphNode_ModifyBone', 'AnimGraphNode_LocalToComponentSpace',
                                  'AnimGraphNode_ComponentToLocalSpace'):
            act('delete_anim_node', anim_blueprint_path=abp, node_guid=node['node_guid'])
    slot = [n for n in nodes_of(abp) if n['node_class'] == 'AnimGraphNode_Slot'][0]['node_guid']
    to_cs = act('add_local_to_component_space', anim_blueprint_path=abp, position_x=300, position_y=300)['node_guid']
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=slot, target_node_guid=to_cs)
    previous = to_cs
    for index, (bone, rotation_property) in enumerate(FLINCH_BONES):
        node = additive_modify_bone(abp, bone, 550 + index * 250, 300, rotation_property)
        act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=previous, target_node_guid=node,
            target_pin='ComponentPose')
        previous = node
    act('add_component_to_local_space', anim_blueprint_path=abp, position_x=1350, position_y=300)
    to_ls = [n for n in nodes_of(abp) if n['node_class'] == 'AnimGraphNode_ComponentToLocalSpace'][0]['node_guid']
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=previous, target_node_guid=to_ls)
    act('wire_anim_node_to_output', anim_blueprint_path=abp, node_name='Component To Local')
    compile_and_save(abp)


# ---------------------------------------------------------------------------------------------------------------
# Guns
# ---------------------------------------------------------------------------------------------------------------
def build_gun(abp, spec):
    for guid in kick_nodes(nodes_of(abp)):
        act('delete_anim_node', anim_blueprint_path=abp, node_guid=guid)
    nodes = nodes_of(abp)
    yaw_guid, pitch_guid = yaw_and_pitch(nodes)
    previous = pitch_guid
    for index, bone in enumerate(spec['bones']):
        node = additive_modify_bone(abp, bone, -560 + index * KICK_NODE_SPACING, 260 + index * 40, 'FireKickRotation',
                                    'FireKickTranslation')
        act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=previous, target_node_guid=node,
            target_pin='ComponentPose')
        previous = node
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=previous, target_node_guid=yaw_guid,
        target_pin='ComponentPose')
    compile_and_save(abp)
    set_kick_defaults(abp, spec)


def source_of(node):
    """The node feeding this node's pose input (bound pins are listed as inputs too)."""
    for pin in node.get('inputs') or []:
        if pin.get('name') in ('ComponentPose', 'LocalPose', 'Source', 'Result') and pin.get('connected_from'):
            return pin['connected_from'][0]['node_guid']
    return None


def yaw_and_pitch(nodes):
    """The root yaw node feeds Component To Local; the elevation node is the first Modify Bone after Local To Component."""
    to_ls = [n for n in nodes if n['node_class'] == 'AnimGraphNode_ComponentToLocalSpace'][0]
    to_cs = [n for n in nodes if n['node_class'] == 'AnimGraphNode_LocalToComponentSpace'][0]
    pitch = [n for n in nodes if n['node_class'] == 'AnimGraphNode_ModifyBone' and source_of(n) == to_cs['node_guid']][0]
    return source_of(to_ls), pitch['node_guid']


def kick_nodes(nodes):
    """Kick nodes from an earlier run: every Modify Bone between the elevation node and the yaw node."""
    by_guid = {n['node_guid']: n for n in nodes}
    yaw_guid, pitch_guid = yaw_and_pitch(nodes)
    found, guid = [], source_of(by_guid[yaw_guid])
    while guid and guid != pitch_guid:
        found.append(guid)
        guid = source_of(by_guid[guid])
    return found


def set_kick_defaults(abp, spec):
    run_python(f"""
import unreal
bp = unreal.load_asset('{abp}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
kick = cdo.get_editor_property('FireKick')
for name, value in (('kick_bone', unreal.Name('{spec['bones'][0]}')), ('gun_frame_socket', unreal.Name('{spec['socket']}')),
                    ('jolt_centimetres', {spec['jolt']}), ('rock_degrees', {spec['rock']}),
                    ('rock_pivot_back_centimetres', {spec['pivot']}), ('muzzle_jump_degrees', {spec['muzzle']}),
                    ('frequency_hz', {spec['hz']}), ('damping_ratio', {spec['damping']})):
    kick.set_editor_property(name, value)
cdo.set_editor_property('FireKick', kick)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
print('KICK', '{abp.split('/')[-1]}', cdo.get_editor_property('FireKick'))
""", 'fire kick defaults ' + abp.split('/')[-1])


def soften_flak_crew():
    act('edit_component_property', blueprint_path=FLAK_TURRET, component_name='PitCrew',
        property_name='CrewFireFlinchStrength', value=f'{FLAK_CREW_FLINCH_STRENGTH:.2f}')
    compiled = act('compile_blueprint', blueprint_path=FLAK_TURRET)
    print('Flak turret compile errors', compiled.get('error_count'))
    act('save_asset', asset_path=FLAK_TURRET)


def quiet_dshk_crew():
    act('edit_component_property', blueprint_path=DSHK_TURRET, component_name='PitCrew',
        property_name='CrewFireFlinchStrength', value='0.0')
    compiled = act('compile_blueprint', blueprint_path=DSHK_TURRET)
    print('DShK turret compile errors', compiled.get('error_count'))
    act('save_asset', asset_path=DSHK_TURRET)


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['crew', 'guns', 'dshk', 'flak']
    # Optional: only the gun anim blueprints whose names contain one of these words.
    only = sys.argv[2].split(',') if len(sys.argv) > 2 else None
    if 'crew' in steps:
        build_crew()
    if 'guns' in steps:
        for path, gun_spec in GUNS.items():
            if only is None or any(word in path for word in only):
                build_gun(path, gun_spec)
    if 'dshk' in steps:
        quiet_dshk_crew()
    if 'flak' in steps:
        soften_flak_crew()
