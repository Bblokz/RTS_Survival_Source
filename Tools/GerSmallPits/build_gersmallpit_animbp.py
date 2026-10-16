"""Builds a small German pit's sockets and animation assets through the editor bridge (UECP direct calls, uecp.py).

  Sockets on Meshes/<Pit>/SK_Ger<Pit>Pit_Gun_Skeleton: PitOrigin on Traverse (gun frame origin on the pit floor; the crew ride on
    it) and Muzzle on Gun, from ue_gersmallpit_import.py's print_socket_transforms.
  AnimBp_Ger<Pit>Pit: parent UGunPitAnimInstance. Base pose -> DefaultSlot -> Elevation Modify Bone (pitch, bound to
    CradlePitchRotation / AimPitchAlpha) -> Traverse Modify Bone (yaw, TurntableYawRotation), both additive in
    component space; the weapon, its rocket crate and the crew on Traverse turn a full circle. Its
    LoadingElevationCurveName is the reload's Ger<Pit>Pit_LoadingElevation curve: the weapon levels for loading.
  Montage_Ger<Pit>Pit_Fire / _Reload: the gun's shot and loading drill in DefaultSlot.
  Crew montages (DefaultSlot, AnimBp_B4Pit_Crew plays DefaultSlot): gunner fire / reload, loader reload.

After reimporting the animations (the import deletes and reimports the sequences): 'base,montages', then
build_gersmallpit_turret.py <Pit> --crew-only. A fresh 'animbp' needs GunPits/build_fire_feedback.py guns <Pit>Pit
again.

Run: python build_gersmallpit_animbp.py Puppchen|Panzerschreck [sockets,animbp,base,montages]
     (scratchpad uecp.py importable; UECP_PORT picks the editor)
"""
import json
import os
import re
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
IMPORT_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ue_gersmallpit_import.py')
QUICK_BLEND_SECONDS = 0.04
PITS = ('Puppchen', 'Panzerschreck')


def names(pit):
    stem = f'Ger{pit}Pit'
    return {'stem': stem, 'skeleton': f'{ROOT}/Meshes/{pit}/SK_{stem}_Gun_Skeleton', 'anim_bp': f'{ROOT}/Anims/AnimBp_{stem}',
            'prefix': f'Anim_{stem}', 'curve': f'{stem}_LoadingElevation'}


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


def add_sockets(pit):
    n = names(pit)
    with open(IMPORT_SCRIPT, encoding='utf-8') as handle:
        output = run_python(f"PIT = '{pit}'\nSTEPS = ['sockets']\n" + handle.read(), 'small pit socket transforms')
    match = re.search(r'GERSMALLPIT_SOCKETS (\{.*\})', output)
    if not match:
        print('FAILED socket transforms', output[:800])
        sys.exit(1)
    for name, socket in json.loads(match.group(1)).items():
        act('add_skeleton_socket', skeleton_path=n['skeleton'], socket_name=name, bone_name=socket['bone'])
        act('set_skeleton_socket_transform', skeleton_path=n['skeleton'], socket_name=name,
            relative_location=socket['relative_location'], relative_rotation=socket['relative_rotation'],
            relative_scale=[1.0, 1.0, 1.0])
        print('socket', name, socket['bone'], socket['relative_location'])
    act('save_asset', asset_path=n['skeleton'])


def modify_bone(abp, bone, x, rotation_property, alpha_property=None):
    node = act('add_modify_bone', anim_blueprint_path=abp, position_x=x, position_y=0)['node_guid']
    for name, value in (('BoneToModify', bone), ('RotationMode', 'BMM_Additive'), ('RotationSpace', 'BCS_ComponentSpace'),
                        ('TranslationMode', 'BMM_Ignore'), ('ScaleMode', 'BMM_Ignore')):
        act('set_anim_node_property', anim_blueprint_path=abp, node_guid=node, property_name=name, property_value=value)
    act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Rotation', property_path=rotation_property)
    if alpha_property:
        act('bind_anim_pin', anim_blueprint_path=abp, node_guid=node, pin='Alpha', property_path=alpha_property)
    return node


def build_anim_blueprint(pit):
    n = names(pit)
    abp = n['anim_bp']
    asset_name = abp.split('/')[-1]
    print(run_python(f"""
import unreal
path = '{abp}'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorAssetLibrary.delete_asset(path)
factory = unreal.AnimBlueprintFactory()
factory.set_editor_property('target_skeleton', unreal.load_asset('{n['skeleton']}'))
factory.set_editor_property('parent_class', unreal.GunPitAnimInstance)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('{asset_name}', '{ROOT}/Anims', unreal.AnimBlueprint, factory)
print('CREATED', asset.get_path_name() if asset else None)
""", 'small pit anim blueprint')[:300])
    for node in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']:
        if node['node_class'] == 'AnimGraphNode_StateMachine':
            act('delete_anim_node', anim_blueprint_path=abp, node_guid=node['node_guid'])
    seq = act('add_anim_node', anim_blueprint_path=abp, node_class='AnimGraphNode_SequencePlayer', position_x=-1400, position_y=0)['node_guid']
    act('set_anim_node_property', anim_blueprint_path=abp, node_guid=seq, property_name='Sequence',
        property_value=f"{ROOT}/Anims/{n['prefix']}_Base")
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
    to_ls = [m for m in act('get_anim_graph_nodes', anim_blueprint_path=abp)['nodes']
             if m['node_class'] == 'AnimGraphNode_ComponentToLocalSpace'][0]['node_guid']
    act('connect_anim_nodes', anim_blueprint_path=abp, source_node_guid=yaw, target_node_guid=to_ls)
    act('wire_anim_node_to_output', anim_blueprint_path=abp, node_name='Component To Local')
    compiled = act('compile_blueprint', blueprint_path=abp)
    print(asset_name, 'compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=abp)
    print(run_python(f"""
import unreal
bp = unreal.load_asset('{abp}')
bp.modify()
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('LoadingElevationCurveName', unreal.Name('{n['curve']}'))
# A class default set from Python only survives through save_packages; checked again after a compile.
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print('CURVE', unreal.get_default_object(bp.generated_class()).get_editor_property('LoadingElevationCurveName'))
""", 'small pit loading curve')[:300])


def set_base_pose(pit):
    """Points the anim blueprint's sequence player at the base pose again after the import replaced the sequence."""
    n = names(pit)
    for node in [m for m in act('get_anim_graph_nodes', anim_blueprint_path=n['anim_bp'])['nodes']
                 if m['node_class'] == 'AnimGraphNode_SequencePlayer']:
        act('set_anim_node_property', anim_blueprint_path=n['anim_bp'], node_guid=node['node_guid'],
            property_name='Sequence', property_value=f"{ROOT}/Anims/{n['prefix']}_Base")
    compiled = act('compile_blueprint', blueprint_path=n['anim_bp'])
    print('base pose set, compile errors', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=n['anim_bp'])


def montages(pit):
    n = names(pit)
    stem = n['stem']
    return {f'{ROOT}/Anims/Montage_{stem}_Fire': f"{ROOT}/Anims/{n['prefix']}_Fire",
            f'{ROOT}/Anims/Montage_{stem}_Reload': f"{ROOT}/Anims/{n['prefix']}_Reload",
            f'{ROOT}/Crew/Montage_{stem}_Gunner_Fire': f"{ROOT}/Crew/{n['prefix']}_Gunner_Fire",
            f'{ROOT}/Crew/Montage_{stem}_Gunner_Reload': f"{ROOT}/Crew/{n['prefix']}_Gunner_Reload",
            f'{ROOT}/Crew/Montage_{stem}_Loader_Reload': f"{ROOT}/Crew/{n['prefix']}_Loader_Reload"}


def build_montages(pit):
    # The import deletes and reimports the sequences, which leaves existing montages pointing at nothing: start clean.
    targets = montages(pit)
    run_python(f"""
import unreal
for path in {list(targets)}:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
""", 'small pit delete montages')
    for path, source in targets.items():
        folder, name = path.rsplit('/', 1)
        result = act('create_montage', source_animation=source, name=name, save_path=folder)
        print('montage', result.get('asset_path'))
    quick = [p for p in targets if p.endswith('_Fire')]
    print(run_python(f"""
import unreal
for path in {quick}:
    montage = unreal.load_asset(path)
    for prop in ('blend_in', 'blend_out'):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property('blend_time', {QUICK_BLEND_SECONDS})
        montage.set_editor_property(prop, blend)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    print('BLEND', path.split('/')[-1], montage.get_editor_property('blend_in').get_editor_property('blend_time'))
""", 'small pit montage blend')[:400])


if __name__ == '__main__':
    pit_name = sys.argv[1] if len(sys.argv) > 1 else ''
    if pit_name not in PITS:
        print('usage: build_gersmallpit_animbp.py Puppchen|Panzerschreck [steps]')
        sys.exit(1)
    steps = sys.argv[2].split(',') if len(sys.argv) > 2 else ['sockets', 'animbp', 'montages']
    if 'sockets' in steps:
        add_sockets(pit_name)
    if 'animbp' in steps:
        build_anim_blueprint(pit_name)
    if 'base' in steps:
        set_base_pose(pit_name)
    if 'montages' in steps:
        build_montages(pit_name)
