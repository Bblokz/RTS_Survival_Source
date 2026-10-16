"""Builds the German Flak 88 emplacement through the editor bridge (UECP direct calls, see the scratchpad uecp.py).
Run ue_flak88pit_import.py in the editor first.

Steps (default: all, in this order):
  sockets      on the pit gun's own skeleton (transforms from ue_flak88pit_import.py's 'sockets' step): the embedded 88's
               fire sockets BulletLaunch0 / BulletOut0, PitOrigin (carriage) and PitStatic (static root)
  animbp       Anims/AnimBp_GerFlak88Pit: parent UGunPitAnimInstance on the pit gun's skeleton. Reference pose -> DefaultSlot ->
               pitch on Flak88_cannon (CradlePitchRotation / AimPitchAlpha) -> yaw on Flak88_Dock
               (TurntableYawRotation), both additive in component space.
  montages     Crew/Montage_GerFlak88Pit_Loader_Reload / _Handler_Reload (DefaultSlot)
  turret       Turrets/RTS_BP_EmbTurret_GerPit_Flak88: a copy of the Pak 40 pit turret (generic turret anim instance,
               pit crew wiring) with the 88, the embedded 88's weapon (Flak37_88MM, AAShell projectile,
               its sound, shell case and heavy impacts), Crew1 loader and Crew2 handler on PitOrigin, the two handover
               rounds, and the ammo runner: Runner (tag PitRunner) with RunnerRound on his shell bone, driven by a
               second crew component PitRunnerCrew on PitStatic (his loop is his idle).
  runner_round sets RunnerRound's offset from the shell bone so it lies where the bake put the round
  bxp          Ger_BXP_GerPit_Flak88: a copy of the German leFH expansion with the pit mesh, BTX_GerFlak88Pit, and the
               turret at the layout's gun_sink below the ground (0: the 88 stands on the ground, its legs level
               with the raised deck)

Run: python build_flak88pit.py [steps,...]   (UECP_PORT picks the editor)
"""
import json
import os
import re
import sys

from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
FLAK88 = '/Game/RTS_Survival/Blueprints/Turrets/EmbeddedTurrets/Flak88'
GUN = ROOT + '/Meshes/Flak88/SK_GerFlak88Pit_Gun'
SKELETON = GUN + '_Skeleton'
ANIM_BP = ROOT + '/Anims/AnimBp_GerFlak88Pit'
CREW = ROOT + '/Crew'
TURRET = ROOT + '/Turrets/RTS_BP_EmbTurret_GerPit_Flak88'
BASE_TURRET = ROOT + '/Turrets/RTS_BP_EmbTurret_GerPit_Pak40'
BXP = ROOT + '/Ger_BXP_GerPit_Flak88'
BASE_BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Childs/LeFH105/RTS_BP_BXP_LeFH105mm'
PIT_MESH = ROOT + '/Meshes/SM_GerFlak88Pit'
ROUND_MESH = ROOT + '/Meshes/SM_GerFlak88Pit_Round'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/GermanSoldier/Meshes/SK_WWII_GermanSoldier_varA'
CREW_ANIM_BP = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/AnimBp_B4Pit_Crew'
PREFIX = 'Anim_GerFlak88Pit'
HERE = os.path.dirname(os.path.abspath(__file__))
IMPORT_SCRIPT = os.path.join(HERE, 'ue_flak88pit_import.py')
CARRY = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/Flak88Pit/Crew/flak88_pit_carry.json'
LAYOUT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/Flak88Pit/flak88_pit_layout.json'
RUNNER_SHELL_BONE = 'ik_foot_root'
# Below this reload time the loading drill would be rushed.
MIN_RELOAD_FOR_DRILL = 3.0
# The barrel clears the wall top by 6.5 cm at 0 degrees: under a degree of depression would put it into the bags.
TURRET_PINS = [('init_child', 'NewRotationSpeed', '35.0'), ('init_child', 'NewMaxPitch', '10.0'),
               ('init_child', 'NewMinPitch', '0.0'), ('init_embedded', 'TurretHeight', '173.0')]
HEAVY = '/Game/RTS_Survival/ImpactSfxVfx/3HeavyCalibre/'
# The embedded 88's impacts per surface, in the Pak turret's map order (Value 0..4).
IMPACTS = [
    (HEAVY + 'Sand/NS_HeavyCaliber_SandImpact.NS_HeavyCaliber_SandImpact',
     HEAVY + 'Sand/Cue_HeavyCaliber_SandImpact.Cue_HeavyCaliber_SandImpact'),
    (HEAVY + 'Default_A/NS_HeavyCaliber_DefaultImpact.NS_HeavyCaliber_DefaultImpact',
     HEAVY + 'Metal/Cue_HeavyCaliber_MetalImpact.Cue_HeavyCaliber_MetalImpact'),
    (HEAVY + 'Default_A/NS_HeavyCaliber_DefaultImpact.NS_HeavyCaliber_DefaultImpact',
     HEAVY + 'Default_A/Cue_HeavyCaliber_DefaultImpact.Cue_HeavyCaliber_DefaultImpact'),
    (HEAVY + 'Default_A/NS_HeavyCaliber_DefaultImpact.NS_HeavyCaliber_DefaultImpact',
     HEAVY + 'Default_A/Cue_HeavyCaliber_DefaultImpact.Cue_HeavyCaliber_DefaultImpact'),
    (HEAVY + 'Stone/NS_HeavyCaliber_StoneImpact.NS_HeavyCaliber_StoneImpact',
     HEAVY + 'Stone/Cue_HeavyCaliber_StoneImpact.Cue_HeavyCaliber_StoneImpact'),
]
LAUNCH_SOUND = '/Game/RTS_Survival/Sounds/WeaponSounds/HeavyCaliber/Flak88/S_FireWeapon_Flak88.S_FireWeapon_Flak88'
SHELL_CASE_VFX = '/Game/RTS_Survival/Blueprints/ProjectilesAndShells/Projectiles/NS_TankCasingBase.NS_TankCasingBase'
# Selection, decal and nav extents: the walls reach 4.74 m from the pivot and the niche 5.9 m.
PIT_RADIUS = 520.0
PIT_HEIGHT = 165.0
SMOKE_RADIUS = 450.0
SMOKES = 5
DECAL_UNITS = 32.0


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text, strict=False)
    if not data.get('ok', True) or data.get('success') is False:
        print('FAILED', action, json.dumps(params)[:300], '->', text[:900])
        sys.exit(1)
    return data


def run_python(code, description):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': description}})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '')
    return json.loads(text, strict=False).get('output_text', text) if text.startswith('{') else text


def asset_ref(path):
    return f'"{path}.{path.split("/")[-1]}"'


def ue_text_vector(v):
    return f'(X={v[0]:.3f},Y={v[1]:.3f},Z={v[2]:.3f})'


# ---------------------------------------------------------------------------------------------------------------
# Gun
# ---------------------------------------------------------------------------------------------------------------
def add_sockets():
    with open(IMPORT_SCRIPT, encoding='utf-8') as handle:
        output = run_python("STEPS = ['sockets']\n" + handle.read(), 'flak88 pit socket transforms')
    match = re.search(r'FLAK88PIT_SOCKETS (\{.*\})', output)
    if not match:
        print('FAILED socket transforms', output[:800])
        sys.exit(1)
    # Mesh-only sockets cannot be made from Python (add_mesh_socket lands on the skeleton in memory only): all go on
    # the pit gun's own skeleton.
    for name, socket in json.loads(match.group(1))['sockets'].items():
        call_tool('call_action', {'action': 'add_skeleton_socket', 'params': {
            'skeleton_path': SKELETON, 'socket_name': name, 'bone_name': socket['bone']}})
        act('set_skeleton_socket_transform', skeleton_path=SKELETON, socket_name=name,
            relative_location=socket['relative_location'], relative_rotation=socket['relative_rotation'],
            relative_scale=[1.0, 1.0, 1.0])
        print('socket', name, socket['bone'], [round(c, 2) for c in socket['relative_location']],
              [round(c, 2) for c in socket['relative_rotation']])
    print(run_python(f"""
import unreal
print('SAVED', unreal.EditorAssetLibrary.save_asset('{SKELETON}', only_if_is_dirty=False))
""", 'flak88 pit save skeleton')[:200])
    # Read the result back: both at the deck top, PitOrigin turned round on the carriage, PitStatic not.
    print(run_python(f"""
import unreal
mesh = unreal.load_asset('{GUN}')
for i in range(mesh.num_sockets()):
    s = mesh.get_socket_by_index(i)
    print('SOCKET', s.get_editor_property('socket_name'), s.get_editor_property('bone_name'),
          s.get_editor_property('relative_location'), s.get_editor_property('relative_rotation'))
""", 'flak88 pit socket check')[:1500])


def modify_bone(bone, x, rotation_property, alpha_property=None):
    node = act('add_modify_bone', anim_blueprint_path=ANIM_BP, position_x=x, position_y=0)['node_guid']
    for name, value in (('BoneToModify', bone), ('RotationMode', 'BMM_Additive'), ('RotationSpace', 'BCS_ComponentSpace'),
                        ('TranslationMode', 'BMM_Ignore'), ('ScaleMode', 'BMM_Ignore')):
        act('set_anim_node_property', anim_blueprint_path=ANIM_BP, node_guid=node, property_name=name, property_value=value)
    act('bind_anim_pin', anim_blueprint_path=ANIM_BP, node_guid=node, pin='Rotation', property_path=rotation_property)
    if alpha_property:
        act('bind_anim_pin', anim_blueprint_path=ANIM_BP, node_guid=node, pin='Alpha', property_path=alpha_property)
    return node


def build_anim_blueprint():
    asset_name = ANIM_BP.split('/')[-1]
    print(run_python(f"""
import unreal
path = '{ANIM_BP}'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorAssetLibrary.delete_asset(path)
factory = unreal.AnimBlueprintFactory()
factory.set_editor_property('target_skeleton', unreal.load_asset('{SKELETON}'))
factory.set_editor_property('parent_class', unreal.GunPitAnimInstance)
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset('{asset_name}', '{ROOT}/Anims', unreal.AnimBlueprint, factory)
print('CREATED', asset.get_path_name() if asset else None)
""", 'flak88 pit anim blueprint')[:300])
    for node in act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']:
        if node['node_class'] == 'AnimGraphNode_StateMachine':
            act('delete_anim_node', anim_blueprint_path=ANIM_BP, node_guid=node['node_guid'])
    # The pit gun has no animations of its own: its reference pose is the rest pose.
    seq = act('add_anim_node', anim_blueprint_path=ANIM_BP, node_class='AnimGraphNode_LocalRefPose', position_x=-1400,
              position_y=0)['node_guid']
    slot = act('add_slot_node', anim_blueprint_path=ANIM_BP, slot_name='DefaultSlot', position_x=-1150, position_y=0)['node_guid']
    to_cs = act('add_local_to_component_space', anim_blueprint_path=ANIM_BP, position_x=-950, position_y=0)['node_guid']
    pitch = modify_bone('Flak88_cannon', -700, 'CradlePitchRotation', 'AimPitchAlpha')
    yaw = modify_bone('Flak88_Dock', -400, 'TurntableYawRotation')
    act('add_component_to_local_space', anim_blueprint_path=ANIM_BP, position_x=-150, position_y=0)
    act('connect_anim_nodes', anim_blueprint_path=ANIM_BP, source_node_guid=seq, target_node_guid=slot, target_pin='Source')
    act('connect_anim_nodes', anim_blueprint_path=ANIM_BP, source_node_guid=slot, target_node_guid=to_cs)
    act('connect_anim_nodes', anim_blueprint_path=ANIM_BP, source_node_guid=to_cs, target_node_guid=pitch, target_pin='ComponentPose')
    act('connect_anim_nodes', anim_blueprint_path=ANIM_BP, source_node_guid=pitch, target_node_guid=yaw, target_pin='ComponentPose')
    to_ls = [m for m in act('get_anim_graph_nodes', anim_blueprint_path=ANIM_BP)['nodes']
             if m['node_class'] == 'AnimGraphNode_ComponentToLocalSpace'][0]['node_guid']
    act('connect_anim_nodes', anim_blueprint_path=ANIM_BP, source_node_guid=yaw, target_node_guid=to_ls)
    act('wire_anim_node_to_output', anim_blueprint_path=ANIM_BP, node_name='Component To Local')
    compiled = act('compile_blueprint', blueprint_path=ANIM_BP)
    print(asset_name, 'compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=ANIM_BP)


def build_montages():
    targets = {f'{CREW}/Montage_GerFlak88Pit_Loader_Reload': f'{CREW}/{PREFIX}_Loader_Reload',
               f'{CREW}/Montage_GerFlak88Pit_Handler_Reload': f'{CREW}/{PREFIX}_Handler_Reload'}
    run_python(f"""
import unreal
for path in {list(targets)}:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
""", 'flak88 pit delete montages')
    for path, source in targets.items():
        folder, name = path.rsplit('/', 1)
        print('montage', act('create_montage', source_animation=source, name=name, save_path=folder).get('asset_path'))


# ---------------------------------------------------------------------------------------------------------------
# Turret
# ---------------------------------------------------------------------------------------------------------------
def duplicate(source, target):
    print(run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{target}'):
    eal.delete_asset('{target}')
print('DUP', eal.duplicate_asset('{source}', '{target}'))
eal.save_asset('{target}', only_if_is_dirty=False)
""", 'flak88 pit duplicate')[:200])


def no_collision(name):
    call_tool('call_action', {'action': 'edit_component_property', 'params': {
        'blueprint_path': TURRET, 'component_name': name, 'property_name': 'BodyInstance',
        'value': '(CollisionEnabled=NoCollision,CollisionProfileName="NoCollision")'}})


def set_components():
    for name in ('Crew3', 'Crew4', 'ReadyCrate'):
        act('delete_component', blueprint_path=TURRET, component_name=name)
    act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name='Turret', mesh_path=GUN, anim_bp_path=ANIM_BP)
    act('set_component_transform', blueprint_path=TURRET, component_name='Turret', location=[0.0, 0.0, 0.0],
        rotation=[0.0, 0.0, 0.0])
    for name in ('Crew1', 'Crew2'):
        act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name=name, mesh_path=SOLDIER,
            anim_bp_path=CREW_ANIM_BP)
        act('set_component_transform', blueprint_path=TURRET, component_name=name, location=[0.0, 0.0, 0.0],
            rotation=[0.0, 0.0, 0.0])
    for name in ('Round1', 'Round2'):
        act('set_static_mesh_component', blueprint_path=TURRET, component_name=name, mesh_path=ROUND_MESH)
    act('add_component', blueprint_path=TURRET, component_class='SkeletalMeshComponent', component_name='Runner')
    act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name='Runner', mesh_path=SOLDIER,
        anim_bp_path=CREW_ANIM_BP)
    act('edit_component_property', blueprint_path=TURRET, component_name='Runner', property_name='ComponentTags',
        value='("PitRunner")')
    act('add_component', blueprint_path=TURRET, component_class='StaticMeshComponent', component_name='RunnerRound',
        attach_to='Runner', socket_name=RUNNER_SHELL_BONE)
    act('set_static_mesh_component', blueprint_path=TURRET, component_name='RunnerRound', mesh_path=ROUND_MESH)
    for name in ('Round1', 'Round2', 'RunnerRound'):
        no_collision(name)
    act('add_component', blueprint_path=TURRET, component_class='GunPitCrewComponent', component_name='PitRunnerCrew')


def crew_settings(carry):
    members = [
        f'(IdleAnimation={asset_ref(f"{CREW}/{PREFIX}_Loader_Idle")},'
        f'ReloadMontage={asset_ref(f"{CREW}/Montage_GerFlak88Pit_Loader_Reload")})',
        f'(IdleAnimation={asset_ref(f"{CREW}/{PREFIX}_Handler_Idle")},'
        f'ReloadMontage={asset_ref(f"{CREW}/Montage_GerFlak88Pit_Handler_Reload")})',
    ]
    # No gun montages: the barrel's recoil is the fire kick on Flak88_GunBarrel (Tools/GunPits/build_fire_feedback.py).
    value = (f'(GunFireMontage=None,CrewAnimations=({",".join(members)}),'
             f'MinReloadTimeForLoadingSequence={MIN_RELOAD_FOR_DRILL:.6f})')
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ReloadSettings', value=value)

    def pose(entry):
        return f'(Location={ue_text_vector(entry["location"])},Axis={ue_text_vector(entry["axis"])})'
    carry_value = (f'(RoundTag="PitRound",HandlerCrewIndex=1,LoaderCrewIndex=0,HandBone="hand_r",'
                   f'HandoverFraction={carry["handover_fraction"]:.4f},RamFraction={carry["ram_fraction"]:.4f},'
                   f'PickFraction={carry["pick_fraction"]:.4f},CrateRound={pose(carry["crate_round"])},'
                   f'IdleRound={pose(carry["idle_round"])},FirstRoundDelaySeconds=0.300000)')
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ShellCarry', value=carry_value)
    for prop, prop_value in (('CrewTag', 'PitCrew'), ('CrewAttachSocket', 'PitOrigin'), ('bKeepCrewBlueprintOffset', 'false'),
                             ('CrewFireFlinchStrength', '1.0')):
        act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name=prop, value=prop_value)
    # The runner: his loop is his idle; no rounds of this component's own, no drill, no brace (not told of shots).
    runner_value = f'(CrewAnimations=((IdleAnimation={asset_ref(f"{CREW}/{PREFIX}_Runner_Loop")})))'
    act('edit_component_property', blueprint_path=TURRET, component_name='PitRunnerCrew', property_name='ReloadSettings',
        value=runner_value)
    act('edit_component_property', blueprint_path=TURRET, component_name='PitRunnerCrew', property_name='ShellCarry',
        value='(RoundTag="None")')
    for prop, prop_value in (('CrewTag', 'PitRunner'), ('CrewAttachSocket', 'PitStatic'), ('bKeepCrewBlueprintOffset', 'false'),
                             ('CrewFireFlinchStrength', '0.0'), ('PropSocketTagPrefix', 'RunnerPitSocket:')):
        act('edit_component_property', blueprint_path=TURRET, component_name='PitRunnerCrew', property_name=prop, value=prop_value)


def find_nodes():
    nodes = act('get_graph_snapshot', blueprint_path=TURRET, graph_name='EventGraph', view='compact')['nodes']

    def detail(node, key):
        return node.get('detail', {}).get(key)

    def one(predicate, label):
        matches = [node for node in nodes if predicate(node)]
        if len(matches) != 1:
            print('FAILED node lookup', label, [m['id'] for m in matches])
            sys.exit(1)
        return matches[0]['id']
    impacts = {}
    for node in nodes:
        if node.get('op') == 'make_struct' and detail(node, 'struct') == 'RTSSurfaceImpactData':
            for pin in node.get('pins', []):
                for link in pin.get('connected_to', []):
                    if link.get('pin', '').startswith('Value '):
                        impacts[int(link['pin'].split(' ')[1])] = node['id']
    return {
        'init_child': one(lambda m: m.get('op') == 'call' and detail(m, 'function') == 'InitChildTurret', 'init child'),
        'init_embedded': one(lambda m: m.get('op') == 'call' and detail(m, 'function') == 'InitEmbeddedTurret', 'init embedded'),
        'init_pit_crew': one(lambda m: m.get('op') == 'call' and detail(m, 'function') == 'InitPitCrew', 'init pit crew'),
        'weapon': one(lambda m: m.get('op') == 'make_struct' and detail(m, 'struct') == 'InitWeaponStateProjectile', 'weapon'),
        'vfx': one(lambda m: m.get('op') == 'make_struct' and detail(m, 'struct') == 'WeaponVFX', 'vfx'),
        'shell': one(lambda m: m.get('op') == 'make_struct' and detail(m, 'struct') == 'WeaponShellCase', 'shell'),
        'impacts': impacts,
    }


def set_weapon_and_wiring():
    ids = find_nodes()
    pins = [(ids[key], pin, value) for key, pin, value in TURRET_PINS] + [
        (ids['weapon'], 'ProjectileSystem', 'AAShell'), (ids['weapon'], 'WeaponName', 'Flak37_88MM'),
        (ids['weapon'], 'FireSocketName', 'BulletLaunch0'),
        (ids['vfx'], 'LaunchSound', LAUNCH_SOUND),
        (ids['shell'], 'ShellCaseSocketName', 'BulletOut0'), (ids['shell'], 'ShellCaseVfx', SHELL_CASE_VFX),
        (ids['shell'], 'LifetimeMlt', '3.0'), (ids['shell'], 'MeshSizeMlt', '0.8'), (ids['shell'], 'SpriteSizeMlt', '0.8'),
        (ids['shell'], 'SpeedMlt', '0.6'),
    ]
    for index, (effect, sound) in enumerate(IMPACTS):
        node = ids['impacts'].get(index)
        if node is None:
            print('FAILED impact slot', index, ids['impacts'])
            sys.exit(1)
        pins += [(node, 'ImpactEffect', effect), (node, 'ImpactSound', sound)]
    for node, pin, value in pins:
        act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    # The runner's crew component starts after the gun crew's.
    act('build_blueprint_graph', blueprint_path=TURRET, graph_name='EventGraph',
        nodes=[{'id': 'rn_pc', 'handle': 'var.get.PitRunnerCrew'}, {'id': 'rn_turret', 'handle': 'var.get.Turret'},
               {'id': 'rn_init', 'handle': 'fn.GunPitCrewComponent.InitPitCrew'}],
        connections=[{'from': ids['init_pit_crew'], 'from_pin': 'then', 'to': 'rn_init', 'to_pin': 'execute'},
                     {'from': 'rn_pc', 'from_pin': 'PitRunnerCrew', 'to': 'rn_init', 'to_pin': 'self'},
                     {'from': 'rn_turret', 'from_pin': 'Turret', 'to': 'rn_init', 'to_pin': 'GunMesh'}])


def compile_and_save(path):
    compiled = act('compile_blueprint', blueprint_path=path)
    print(path.split('/')[-1], 'compile errors', compiled.get('error_count'), compiled.get('errors'), 'warnings',
          compiled.get('warning_count'))
    act('save_asset', asset_path=path)


def build_turret():
    with open(CARRY) as handle:
        carry = json.load(handle)
    duplicate(BASE_TURRET, TURRET)
    set_components()
    crew_settings(carry)
    set_weapon_and_wiring()
    compile_and_save(TURRET)


def place_runner_round():
    """RunnerRound's offset from the shell bone: the bake keys the bone onto the round, but the bone's axes differ
    from Blender's after the FBX export. Solved from the runner's loop at a carry moment of the bake."""
    with open(CARRY) as handle:
        reference = next(r for r in json.load(handle)['runner']['reference'] if r['shown'])
    output = run_python(f"""
import unreal
lib = unreal.AnimationLibrary
seq = unreal.load_asset('{CREW}/{PREFIX}_Runner_Loop')
t = {reference['time']}
bone = lib.get_bone_pose_for_time(seq, '{RUNNER_SHELL_BONE}', t, False)
parent = lib.get_bone_pose_for_time(seq, 'root', t, False)
component = bone.multiply(parent)
axis = unreal.Vector(*{reference['axis']})
rotation = unreal.MathLibrary.make_rot_from_xz(axis, unreal.Vector(0.0, 0.0, 1.0))
wanted = unreal.Transform(unreal.Vector(*{reference['centre']}), rotation, unreal.Vector(1.0, 1.0, 1.0))
relative = wanted.multiply(component.inverse())
r = relative.rotation.rotator()
print('RUNNER_ROUND', relative.translation.x, relative.translation.y, relative.translation.z, r.pitch, r.yaw, r.roll,
      'bone_scale', component.scale3d.x)
""", 'flak88 runner round offset')
    match = re.search(r'RUNNER_ROUND (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) bone_scale (\S+)', output)
    if not match:
        print('FAILED runner round offset', output[:800])
        sys.exit(1)
    x, y, z, pitch, yaw, roll, scale = (float(v) for v in match.groups())
    print('RunnerRound offset', [round(x, 2), round(y, 2), round(z, 2)], [round(pitch, 2), round(yaw, 2), round(roll, 2)],
          'bone scale', round(scale, 3))
    act('set_component_transform', blueprint_path=TURRET, component_name='RunnerRound', location=[x, y, z],
        rotation=[pitch, yaw, roll])
    compile_and_save(TURRET)


# ---------------------------------------------------------------------------------------------------------------
# Expansion
# ---------------------------------------------------------------------------------------------------------------
def init_node():
    nodes = act('get_graph_snapshot', blueprint_path=BXP, graph_name='EventGraph', view='compact')['nodes']
    matches = [n['id'] for n in nodes if n.get('op') == 'call' and n.get('detail', {}).get('function') == 'BP_InitBuildingExp']
    if len(matches) != 1:
        print('FAILED BP_InitBuildingExp lookup', matches)
        sys.exit(1)
    return matches[0]


def build_bxp():
    with open(LAYOUT) as handle:
        gun_sink = json.load(handle)['gun_sink']
    duplicate(BASE_BXP, BXP)
    mesh_ref = f"{PIT_MESH}.{PIT_MESH.split('/')[-1]}"
    node = init_node()
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewBuildingExpansionType', 'BTX_GerFlak88Pit'), ('NewSmokeRadius', f'{SMOKE_RADIUS:.1f}'),
                       ('NewAmountSmokes', str(SMOKES))):
        act('set_pin_default', blueprint_path=BXP, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    # A placed pit never runs construction and shows this template mesh.
    act('set_static_mesh_component', blueprint_path=BXP, component_name='BuildingMeshComponent', mesh_path=PIT_MESH)
    act('edit_component_property', blueprint_path=BXP, component_name='Howitzer', property_name='ChildActorClass',
        value=f"{TURRET}.{TURRET.split('/')[-1]}_C")
    act('set_component_transform', blueprint_path=BXP, component_name='Howitzer', location=[0.0, 0.0, -gun_sink])
    decal_scale = 2.0 * PIT_RADIUS / DECAL_UNITS
    for component, prop, value in (
            ('bpSelectionArea', 'BoxExtent', f'(X={PIT_RADIUS * 0.85:.1f},Y={PIT_RADIUS * 0.85:.1f},Z={PIT_HEIGHT * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeScale3D', '(X=1.0,Y=1.0,Z=1.0)'),
            ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
            ('ProgressBarWidget', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={PIT_HEIGHT + 230.0:.1f})'),
            ('NavModifier', 'FailsafeExtent', f'(X={PIT_RADIUS:.1f},Y={PIT_RADIUS:.1f},Z={PIT_HEIGHT:.1f})'),
            ('RTS', 'OwningPlayer', '1')):
        act('edit_component_property', blueprint_path=BXP, component_name=component, property_name=prop, value=value)
    compile_and_save(BXP)


STEPS = {'sockets': add_sockets, 'animbp': build_anim_blueprint, 'montages': build_montages, 'turret': build_turret,
         'runner_round': place_runner_round, 'bxp': build_bxp}

if __name__ == '__main__':
    chosen = sys.argv[1].split(',') if len(sys.argv) > 1 else list(STEPS)
    for step in chosen:
        print('==', step)
        STEPS[step]()
