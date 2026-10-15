"""Builds the Ampulomet pit's sockets and animation assets through the editor bridge (UECP direct calls, see uecp.py).

  Sockets on SK_AmpulometPit_Gun_Skeleton: PitOrigin on Traverse (gun frame origin on the pit floor; the crew ride on
    it) and Muzzle on Gun, from ue_ampulometpit_import.py's print_socket_transforms.
  AnimBp_AmpulometPit: parent UGunPitAnimInstance. Base pose -> DefaultSlot -> Elevation Modify Bone (pitch, bound to
    CradlePitchRotation / AimPitchAlpha) -> Traverse Modify Bone (yaw, TurntableYawRotation), both additive in
    component space; the tripod, crates and crew on Traverse turn a full circle on the gun table. Its
    LoadingElevationCurveName is the reload's AmpulometPit_LoadingElevation curve, so the tube levels for loading.
  Montage_AmpulometPit_Fire / _Reload: the gun's shot and breech drill in DefaultSlot.
  Crew montages (DefaultSlot, AnimBp_B4Pit_Crew plays DefaultSlot): gunner fire / reload, loader reload.

After reimporting the animations (the import deletes and reimports the sequences): 'base,montages', then
build_ampulometpit_turret.py --crew-only. A fresh 'animbp' needs GunPits/build_fire_feedback.py guns AmpulometPit again.

Run: python build_ampulometpit_animbp.py [sockets,animbp,base,montages]   (scratchpad uecp.py importable; UECP_PORT picks
the editor)
"""
import json
import os
import re
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusAmpulometPit'
SKELETON = ROOT + '/Meshes/SK_AmpulometPit_Gun_Skeleton'
ANIM_BP = ROOT + '/Anims/AnimBp_AmpulometPit'
IMPORT_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ue_ampulometpit_import.py')
LOADING_CURVE = 'AmpulometPit_LoadingElevation'
GUN_MONTAGES = {'Montage_AmpulometPit_Fire': ROOT + '/Anims/Anim_AmpulometPit_Fire',
                'Montage_AmpulometPit_Reload': ROOT + '/Anims/Anim_AmpulometPit_Reload'}
CREW_MONTAGES = {'Montage_AmpulometPit_Gunner_Fire': ROOT + '/Crew/Anim_AmpulometPit_Gunner_Fire',
                 'Montage_AmpulometPit_Gunner_Reload': ROOT + '/Crew/Anim_AmpulometPit_Gunner_Reload',
                 'Montage_AmpulometPit_Loader_Reload': ROOT + '/Crew/Anim_AmpulometPit_Loader_Reload'}
QUICK_BLEND_MONTAGES = ('Montage_AmpulometPit_Fire', 'Montage_AmpulometPit_Gunner_Fire')
QUICK_BLEND_SECONDS = 0.04


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
    return json.loads(text, strict=False).get('output_text', text) if text.startswith('{') else text


def add_sockets():
    with open(IMPORT_SCRIPT, encoding='utf-8') as handle:
        output = run_python("STEPS = ['sockets']\n" + handle.read(), 'Ampulomet pit socket transforms')
    match = re.search(r'AMPPIT_SOCKETS (\{.*\})', output)
    if not match:
        print('FAILED socket transforms', output[:800])
        sys.exit(1)
    sockets = json.loads(match.group(1))
    for name, socket in sockets.items():
        act('add_skeleton_socket', skeleton_path=SKELETON, socket_name=name, bone_name=socket['bone'])
        act('set_skeleton_socket_transform', skeleton_path=SKELETON, socket_name=name,
            relative_location=socket['relative_location'], relative_rotation=socket['relative_rotation'],
            relative_scale=[1.0, 1.0, 1.0])
        print('socket', name, socket['bone'], socket['relative_location'])
    act('save_asset', asset_path=SKELETON)


def create_anim_blueprint():
    print(run_python(f"""
import unreal
path = '{ANIM_BP}'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorAssetLibrary.delete_asset(path)
factory = unreal.AnimBlueprintFactory()
factory.set_editor_property('target_skeleton', unreal.load_asset('{SKELETON}'))
factory.set_editor_property('parent_class', unreal.GunPitAnimInstance)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AnimBp_AmpulometPit', '{ROOT}/Anims', unreal.AnimBlueprint, factory)
print('CREATED', asset.get_path_name() if asset else None)
""", 'Ampulomet pit anim blueprint')[:300])


def modify_bone(abp, bone, x, rotation_property, alpha_property=None):
    node = act('add_modify_bone', anim_blueprint_path=abp, position_x=x, position_y=0)['node_guid']
    for name, value in (('BoneToModify', bone), ('RotationMode', 'BMM_Additive'), ('RotationSpace', 'BCS_ComponentSpace'),
                        ('TranslationMode', 'BMM_Ignore'), ('ScaleMode', 'BMM_Ignore')):
        act('set_anim_node_property', anim_blueprint_path=abp, node_guid=node, property_name=name, property_value=value)
    act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Rotation', property_path=rotation_property)
    if alpha_property:
        act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Alpha', property_path=alpha_property)
    return node


def set_loading_curve():
    print(run_python(f"""
import unreal
bp = unreal.load_asset('{ANIM_BP}')
bp.modify()
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('LoadingElevationCurveName', unreal.Name('{LOADING_CURVE}'))
# A class default set from Python only survives through save_packages; checked again after a compile.
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print('CURVE', unreal.get_default_object(bp.generated_class()).get_editor_property('LoadingElevationCurveName'))
""", 'Ampulomet pit loading curve')[:300])


def build_anim_blueprint():
    create_anim_blueprint()
    abp = ANIM_BP
    for node in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']:
        if node['node_class'] == 'AnimGraphNode_StateMachine':
            act('delete_anim_node', anim_blueprint_path=abp, node_guid=node['node_guid'])
    seq = act('add_anim_node', anim_blueprint_path=abp, node_class='AnimGraphNode_SequencePlayer', position_x=-1400, position_y=0)['node_guid']
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='Sequence',
        property_value=ROOT + '/Anims/Anim_AmpulometPit_Base')
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
    print('AnimBp_AmpulometPit compile', compiled.get('error_count'), compiled.get('warning_count'), compiled.get('errors'))
    act('save_asset', asset_path=abp)
    set_loading_curve()


def set_base_pose():
    """Points the anim blueprint's sequence player at the base pose again after the import replaced the sequence."""
    players = [n for n in act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']
               if n['node_class'] == 'AnimGraphNode_SequencePlayer']
    for node in players:
        act('set_anim_node_property', anim_blueprint_path=ANIM_BP, node_guid=node['node_guid'], property_name='Sequence',
            property_value=ROOT + '/Anims/Anim_AmpulometPit_Base')
    compiled = act('compile_blueprint', blueprint_path=ANIM_BP)
    print('AnimBp_AmpulometPit base pose set, compile errors', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=ANIM_BP)


def build_montages():
    # The import deletes and reimports the sequences, which leaves existing montages pointing at nothing: start clean
    # (then rerun build_ampulometpit_turret.py --crew-only, whose crew settings reference these montages).
    paths = [f'{ROOT}/{folder}/{name}' for folder, montages in (('Anims', GUN_MONTAGES), ('Crew', CREW_MONTAGES))
             for name in montages]
    run_python(f"""
import unreal
for path in {paths}:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
""", 'Ampulomet pit delete montages')
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
""", 'Ampulomet pit montage blend')[:300])


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['sockets', 'animbp', 'montages']
    if 'sockets' in steps:
        add_sockets()
    if 'animbp' in steps:
        build_anim_blueprint()
    if 'base' in steps:
        set_base_pose()
    if 'montages' in steps:
        build_montages()
