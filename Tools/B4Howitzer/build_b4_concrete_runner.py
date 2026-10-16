"""Adds the ammo runner to the B-4 and ML-20 concrete pits, through the editor bridge (UECP direct calls).

The concrete pits get turret copies of their own, so the earth pits (Rus_BXP_B4HowitzerPit, Rus_BXP_ML20Pit and
their turrets) stay as they are:
  B4_203MM/Pit/RTS_BP_EmbTurret_B4Pit_Concrete       copy of RTS_BP_EmbTurret_B4Pit
  ML20_152MM/Pit/RTS_BP_EmbTurret_ML20Pit_Concrete   copy of RTS_BP_EmbTurret_ML20Pit
Each copy adds:
  ReadyRack, ReadyShell1/2   a second shell rack at the turntable's rim (tag B4PitSocket:PitOrigin), turned copies of
                             ShellRack / RackShell1/2; its top shell is the runner's
  Runner                     a Russian soldier (tag PitRunner) playing Anim_<Pit>PitRunner_Loop
  RunnerShell                the shell he carries, on his spare bone ik_foot_root (offset solved from the loop)
  PitRunnerCrew              a second UGunPitCrewComponent that puts him on PitOrigin and loops his animation;
                             InitPitCrew is called after the gun crew's in BeginPlay
The loops are baked by b4_pit_runner.py (Marketplace/Scritps/AnimTeamWeapons/B4_203mm), which also writes the
ready rack placement and the shell poses read here.

Steps (default anims,turret,bxp): anims, turret (also places the shell), shell (the shell offset alone), bxp
Run: python build_b4_concrete_runner.py [steps] [B4,ML20]   (scratchpad uecp.py importable; UECP_PORT picks an editor)
"""
import json
import math
import re
import sys

from uecp import call_tool

EXPORTS = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)'
TEAM_WEAPONS = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier'
CREW_ANIM_BP = TEAM_WEAPONS + '/B4_203MM/Pit/AnimBp_B4Pit_Crew'
RUNNER_SHELL_BONE = 'ik_foot_root'
PITS = {
    'B4': {
        'pit': TEAM_WEAPONS + '/B4_203MM/Pit',
        'base_turret': 'RTS_BP_EmbTurret_B4Pit', 'turret': 'RTS_BP_EmbTurret_B4Pit_Concrete',
        'bxp': 'Rus_BXP_B4HowitzerConcretePit', 'anim': 'Anim_B4PitRunner_Loop',
        'export_dir': EXPORTS + '/B4_203mm', 'json': 'b4_pit_runner.json',
    },
    'ML20': {
        'pit': TEAM_WEAPONS + '/ML20_152MM/Pit',
        'base_turret': 'RTS_BP_EmbTurret_ML20Pit', 'turret': 'RTS_BP_EmbTurret_ML20Pit_Concrete',
        'bxp': 'Rus_BXP_ML20ConcretePit', 'anim': 'Anim_ML20PitRunner_Loop',
        'export_dir': EXPORTS + '/ML20_152mm', 'json': 'ml20_pit_runner.json',
    },
}
NO_COLLISION = '(CollisionEnabled=NoCollision,CollisionProfileName="NoCollision")'


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
    text = json.loads(result['result']['content'][0]['text'], strict=False)
    output = text.get('output_text') or text.get('result') or ''
    print(output.strip()[:2000])
    if not text.get('ok', True):
        sys.exit(1)
    return output


def asset_ref(path):
    return f'"{path}.{path.split("/")[-1]}"'


def load_runner(cfg):
    with open(f'{cfg["export_dir"]}/Crew/{cfg["json"]}') as handle:
        return json.load(handle)


def turret_path(cfg):
    return f'{cfg["pit"]}/{cfg["turret"]}'


def anim_path(cfg):
    return f'{cfg["pit"]}/Crew/{cfg["anim"]}'


# ---------------------------------------------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------------------------------------------
def import_anim(cfg):
    fbx = f'{cfg["export_dir"]}/Crew/Anims/{cfg["anim"]}.fbx'
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
path = '{anim_path(cfg)}'
# A reimport cannot change the sample rate of an existing sequence; it is generated only.
if eal.does_asset_exist(path):
    eal.delete_asset(path)
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
options.import_mesh = False
options.import_animations = True
options.import_materials = False
options.import_textures = False
options.skeleton = unreal.load_asset('{INFANTRY_SKELETON}')
anim_data = options.anim_sequence_import_data
anim_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
anim_data.set_editor_property('remove_redundant_keys', False)
anim_data.set_editor_property('import_bone_tracks', True)
task = unreal.AssetImportTask()
task.filename = r'{fbx}'
task.destination_path = '{cfg["pit"]}/Crew'
task.destination_name = '{cfg["anim"]}'
task.automated = True
task.replace_existing = True
task.save = True
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sequence = unreal.load_asset(path)
# He walks round the turntable through his root bone; the mesh is not a character.
sequence.set_editor_property('enable_root_motion', False)
eal.save_loaded_asset(sequence)
print('ANIM', path, sequence.get_play_length())
""", 'runner anim import ' + cfg['anim'])


def rack_layout(cfg):
    """The shell rack's and its two lower shells' Blueprint transforms, and the top shell's mesh."""
    output = run_python(f"""
import unreal
bp = unreal.load_asset('{turret_path(cfg)}')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj = lib.get_object(lib.get_data(h))
    if isinstance(obj, unreal.StaticMeshComponent) and obj.get_name().split('_GEN')[0] in ('ShellRack', 'RackShell1', 'RackShell2', 'RackShell3'):
        l = obj.get_editor_property('relative_location'); r = obj.get_editor_property('relative_rotation')
        b = obj.static_mesh.get_bounding_box()
        print('RACK', obj.get_name().split('_GEN')[0], l.x, l.y, l.z, r.yaw, obj.static_mesh.get_path_name().split('.')[0],
              (b.min.y + b.max.y) * 0.5)
""", 'runner rack layout')
    rack = {}
    for name, x, y, z, yaw, mesh, centre_y in re.findall(r'RACK (\S+) (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)', output):
        rack[name] = {'location': (float(x), float(y), float(z)), 'yaw': float(yaw), 'mesh': mesh,
                      'mesh_centre_y': float(centre_y)}
    if len(rack) != 4:
        print('FAILED rack layout', rack)
        sys.exit(1)
    return rack


def ready_rack_transforms(rack, runner):
    """ReadyRack and ReadyShell1/2: the shell rack and its lower shells turned onto the ready rack's place."""
    origin = rack['ShellRack']['location']
    ready = runner['ready_rack']
    yaw = ready['yaw']
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    transforms = {'ReadyRack': ((ready['origin'][0], ready['origin'][1], origin[2]), rack['ShellRack']['yaw'] + yaw)}
    for source, target in (('RackShell1', 'ReadyShell1'), ('RackShell2', 'ReadyShell2')):
        x, y, z = rack[source]['location']
        rx, ry = x - origin[0], y - origin[1]
        transforms[target] = ((ready['origin'][0] + rx * c - ry * s, ready['origin'][1] + rx * s + ry * c, z),
                              rack[source]['yaw'] + yaw)
    return transforms


def add_component(turret, component_class, name, **extra):
    # Tolerant: a rerun finds the component already there.
    result = call_tool('call_action', {'action': 'add_component', 'params': {
        'blueprint_path': turret, 'component_class': component_class, 'component_name': name, **extra}})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '')
    if '"ok": false' in text.replace('":false', '": false') and 'exist' not in text.lower():
        print('FAILED add_component', name, text[:600])
        sys.exit(1)


def build_turret(cfg):
    runner = load_runner(cfg)
    turret = turret_path(cfg)
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{turret}'):
    eal.delete_asset('{turret}')
print('DUP', eal.duplicate_asset('{cfg["pit"]}/{cfg["base_turret"]}', '{turret}'))
eal.save_asset('{turret}', only_if_is_dirty=False)
""", 'runner turret duplicate ' + cfg['turret'])
    rack = rack_layout(cfg)
    shell_mesh = rack['RackShell3']['mesh']
    for name, (location, yaw) in ready_rack_transforms(rack, runner).items():
        add_component(turret, 'StaticMeshComponent', name)
        act('set_static_mesh_component', blueprint_path=turret, component_name=name,
            mesh_path=rack['ShellRack']['mesh'] if name == 'ReadyRack' else shell_mesh)
        act('set_component_transform', blueprint_path=turret, component_name=name, location=list(location),
            rotation=[0.0, yaw, 0.0])
        act('edit_component_property', blueprint_path=turret, component_name=name, property_name='ComponentTags',
            value='("B4PitSocket:PitOrigin")')
        act('edit_component_property', blueprint_path=turret, component_name=name, property_name='BodyInstance',
            value=NO_COLLISION)
    add_component(turret, 'SkeletalMeshComponent', 'Runner')
    act('set_skeletal_mesh_component', blueprint_path=turret, component_name='Runner', mesh_path=SOLDIER,
        anim_bp_path=CREW_ANIM_BP)
    act('set_component_transform', blueprint_path=turret, component_name='Runner', location=[0.0, 0.0, 0.0],
        rotation=[0.0, 0.0, 0.0])
    act('edit_component_property', blueprint_path=turret, component_name='Runner', property_name='ComponentTags',
        value='("PitRunner")')
    add_component(turret, 'StaticMeshComponent', 'RunnerShell', attach_to='Runner', socket_name=RUNNER_SHELL_BONE)
    act('set_static_mesh_component', blueprint_path=turret, component_name='RunnerShell', mesh_path=shell_mesh)
    act('edit_component_property', blueprint_path=turret, component_name='RunnerShell', property_name='BodyInstance',
        value=NO_COLLISION)
    add_component(turret, 'GunPitCrewComponent', 'PitRunnerCrew')
    # His loop is his idle: no rounds of this component's own, no drill, no brace (it is not told of shots).
    act('edit_component_property', blueprint_path=turret, component_name='PitRunnerCrew', property_name='ReloadSettings',
        value=f'(CrewAnimations=((IdleAnimation={asset_ref(anim_path(cfg))})))')
    act('edit_component_property', blueprint_path=turret, component_name='PitRunnerCrew', property_name='ShellCarry',
        value='(RoundTag="None")')
    for prop, value in (('CrewTag', 'PitRunner'), ('CrewAttachSocket', 'PitOrigin'), ('bKeepCrewBlueprintOffset', 'false'),
                        ('CrewFireFlinchStrength', '0.0'), ('PropSocketTagPrefix', 'RunnerPitSocket:')):
        act('edit_component_property', blueprint_path=turret, component_name='PitRunnerCrew', property_name=prop,
            value=value)
    wire_runner_init(turret)
    compile_and_save(turret)
    place_runner_shell(cfg, rack)


def wire_runner_init(turret):
    nodes = act('get_graph_snapshot', blueprint_path=turret, graph_name='EventGraph', view='compact')['nodes']
    inits = [n for n in nodes if n.get('op') == 'call' and n.get('detail', {}).get('function') == 'InitPitCrew']
    if len(inits) != 1:
        print('FAILED InitPitCrew lookup', [n['id'] for n in inits])
        sys.exit(1)
    # The runner's crew component starts after the gun crew's.
    act('build_blueprint_graph', blueprint_path=turret, graph_name='EventGraph',
        nodes=[{'id': 'rn_pc', 'handle': 'var.get.PitRunnerCrew'}, {'id': 'rn_turret', 'handle': 'var.get.Turret'},
               {'id': 'rn_init', 'handle': 'fn.GunPitCrewComponent.InitPitCrew'}],
        connections=[{'from': inits[0]['id'], 'from_pin': 'then', 'to': 'rn_init', 'to_pin': 'execute'},
                     {'from': 'rn_pc', 'from_pin': 'PitRunnerCrew', 'to': 'rn_init', 'to_pin': 'self'},
                     {'from': 'rn_turret', 'from_pin': 'Turret', 'to': 'rn_init', 'to_pin': 'GunMesh'}])


def compile_and_save(path):
    compiled = act('compile_blueprint', blueprint_path=path)
    print(path.split('/')[-1], 'compile errors', compiled.get('error_count'), compiled.get('errors'), 'warnings',
          compiled.get('warning_count'))
    act('save_asset', asset_path=path)


def place_runner_shell(cfg, rack=None):
    """RunnerShell's offset from the shell bone: the bake keys the bone onto the shell (bone X = the shell mesh's
    +Y), but the bone's axes differ from Blender's after the FBX export. Solved at the first carry reference and
    checked against the others."""
    runner = load_runner(cfg)
    rack = rack or rack_layout(cfg)
    output = run_python(f"""
import unreal
lib = unreal.AnimationLibrary
seq = unreal.load_asset('{anim_path(cfg)}')
up = unreal.Vector(0.0, 0.0, 1.0)
mesh_centre = unreal.Vector(0.0, {rack['RackShell3']['mesh_centre_y']}, 0.0)
references = {runner['reference']!r}

def bone_at(t):
    bone = lib.get_bone_pose_for_time(seq, '{RUNNER_SHELL_BONE}', t, False)
    return bone.multiply(lib.get_bone_pose_for_time(seq, 'root', t, False))

def wanted(reference):
    rotation = unreal.MathLibrary.make_rot_from_yz(unreal.Vector(*reference['axis']), up)
    # The mesh centre lies on the mesh's +Y axis.
    location = unreal.Vector(*reference['centre']) - rotation.get_right_vector() * mesh_centre.y
    return unreal.Transform(location, rotation, unreal.Vector(1.0, 1.0, 1.0))

relative = wanted(references[0]).multiply(bone_at(references[0]['time']).inverse())
for reference in references[1:]:
    placed = relative.multiply(bone_at(reference['time']))
    centre = placed.transform_location(mesh_centre)
    axis = placed.rotation.get_axis_y()
    error = (centre - unreal.Vector(*reference['centre'])).length()
    print('CHECK', reference['time'], round(error, 2), 'axis', round(axis.x, 3), round(axis.y, 3), round(axis.z, 3),
          'want', reference['axis'])
r = relative.rotation.rotator()
print('RUNNER_SHELL', relative.translation.x, relative.translation.y, relative.translation.z, r.pitch, r.yaw, r.roll)
""", 'runner shell offset ' + cfg['turret'])
    match = re.search(r'RUNNER_SHELL (\S+) (\S+) (\S+) (\S+) (\S+) (\S+)', output)
    if not match:
        print('FAILED runner shell offset', output[:800])
        sys.exit(1)
    x, y, z, pitch, yaw, roll = (float(v) for v in match.groups())
    print('RunnerShell offset', [round(x, 2), round(y, 2), round(z, 2)], [round(pitch, 2), round(yaw, 2), round(roll, 2)])
    act('set_component_transform', blueprint_path=turret_path(cfg), component_name='RunnerShell', location=[x, y, z],
        rotation=[pitch, yaw, roll])
    compile_and_save(turret_path(cfg))


def point_bxp(cfg):
    bxp = f'{cfg["pit"]}/{cfg["bxp"]}'
    act('edit_component_property', blueprint_path=bxp, component_name='Howitzer', property_name='ChildActorClass',
        value=f'{turret_path(cfg)}.{cfg["turret"]}_C')
    compile_and_save(bxp)
    print(cfg['bxp'], 'Howitzer ->', act('get_component_property', blueprint_path=bxp, component_name='Howitzer',
                                         property_name='ChildActorClass').get('value'))


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['anims', 'turret', 'bxp']
    pits = sys.argv[2].split(',') if len(sys.argv) > 2 else ['B4', 'ML20']
    for pit_name in pits:
        for step in steps:
            {'anims': import_anim, 'turret': build_turret, 'shell': place_runner_shell, 'bxp': point_bxp}[step](PITS[pit_name])
