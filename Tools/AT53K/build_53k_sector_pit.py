"""Builds the Soviet 53-K sector pit through the editor bridge (UECP direct calls, see uecp.py).

The sector pit holds the 53-K pit's gun, crew and drill in a small square sandbag wall; its turret only traverses
SECTOR_HALF_DEGREES either side of the pit's forward, so the pit covers one sector and must be placed facing it.

  Crew/Anim_53KSectorPit_Handler_*      the shell handler's compact drill and idle (at53k_sector_crew.py),
                                        Crew/Montage_53KSectorPit_Handler_Reload
  Meshes/SM_53KSectorPit                the square wall (at53k_sector_pit_build.py), MI_Rus_SandBag_01, complex collision
  Turrets/RTS_BP_EmbTurret_Rus53KSectorPit
                                        a copy of the 53-K pit turret: the same gun, anim BP, loader and handover; the
                                        handler's new drill, the crate (SM_TankAmmoCrates_A) and the commander pulled
                                        in; InitEmbeddedTurret as an assault turret with +-SECTOR_HALF_DEGREES and the
                                        CDO's bIsFixedEmplacement, so the limits are a fixed arc (no base turn, no
                                        targets outside it)
  Rus_BXP_53KSectorPit                  a copy of the 53-K pit expansion (enemy owned, engages when placed), type
                                        BTX_Rus53KSectorPit, its selection, decal and nav boxes fitted to the wall

The anim BP, fire montage and PitOrigin53K socket are the 53-K pit's (build_53k_pit.py builds them).

Run: python build_53k_sector_pit.py [crew,mesh,turret,bxp]   (scratchpad uecp.py importable; UECP_PORT picks an
editor other than the 9855 broker's)
"""
import json
import math
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/45mmAT_53K/Exports'
PIT_TURRET = ROOT + '/Turrets/RTS_BP_EmbTurret_Rus53KPit'
PIT_BXP = ROOT + '/Rus_BXP_53KPit'
TURRET = ROOT + '/Turrets/RTS_BP_EmbTurret_Rus53KSectorPit'
BXP = ROOT + '/Rus_BXP_53KSectorPit'
MESH = 'SM_53KSectorPit'
HANDLER_ANIMS = ('Anim_53KSectorPit_Handler_Reload', 'Anim_53KSectorPit_Handler_Idle')
HANDLER_MONTAGE = 'Montage_53KSectorPit_Handler_Reload'
SANDBAG_MATERIAL = '/Game/SandBagPack/Materials/MI_Rus_SandBag_01'
CRATE_MESH = '/Game/MillitaryBase/Meshes/SM_TankAmmoCrates_A'
SECTOR_HALF_DEGREES = 45.0
# The front wall is lower than the bore; the barrel's lowest pitch keeps the shot clear of it.
MIN_PITCH = -5.0
MIN_RELOAD_FOR_DRILL = 3.0
# The wall in pit space (at53k_sector_pit_build.py prints it): x from the back to the front, half its width, height.
PIT_BACK = -446.0
PIT_FRONT = 273.0
PIT_HALF_WIDTH = 452.0
PIT_HEIGHT = 72.0
SMOKE_RADIUS = 400.0
SMOKES = 3
DECAL_UNITS = 32.0
EXPANSION_TYPE = 'BTX_RUS53K_SECTOR_PIT'


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
    print(text[:500])
    return text


def load_json(name):
    with open(f'{SOURCE}/Crew/{name}') as handle:
        return json.load(handle)


def duplicate(source, target):
    # An existing copy is reused: deleting a loaded Blueprint opens a confirmation a windowless editor cannot show.
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if not eal.does_asset_exist('{target}'):
    print('DUP', eal.duplicate_asset('{source}', '{target}'))
    eal.save_asset('{target}', only_if_is_dirty=False)
""", '53K sector pit duplicate ' + target.split('/')[-1])


def import_crew():
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
skeleton = unreal.load_asset('{INFANTRY_SKELETON}')
for name in {HANDLER_ANIMS!r}:
    path = '{ROOT}/Crew/' + name
    # Reimporting over a sequence of another length fails; these assets are generated only, so start clean.
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.skeleton = skeleton
    data = options.anim_sequence_import_data
    data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.set_editor_property('remove_redundant_keys', False)
    data.set_editor_property('import_bone_tracks', True)
    task = unreal.AssetImportTask()
    task.filename = '{SOURCE}/Crew/' + name + '.fbx'
    task.destination_path = '{ROOT}/Crew'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sequence = unreal.load_asset(path)
    # The handler walks through the pit on his root bone; the crew meshes are not characters.
    sequence.set_editor_property('enable_root_motion', False)
    eal.save_asset(path, only_if_is_dirty=False)
    print('SECTORPIT_CREW', name, round(sequence.get_play_length(), 2))
""", '53K sector pit handler animations')
    act('create_montage', source_animation=f'{ROOT}/Crew/{HANDLER_ANIMS[0]}', name=HANDLER_MONTAGE, save_path=ROOT + '/Crew')
    act('save_asset', asset_path=f'{ROOT}/Crew/{HANDLER_MONTAGE}')


def import_mesh():
    run_python(f"""
import unreal
task = unreal.AssetImportTask()
task.filename = '{SOURCE}/{MESH}.fbx'
task.destination_path = '{ROOT}/Meshes'
task.destination_name = '{MESH}'
task.automated = True
task.replace_existing = True
task.save = True
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_mesh = True
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
options.static_mesh_import_data.set_editor_property('combine_meshes', True)
options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
path = '{ROOT}/Meshes/{MESH}'
mesh = unreal.load_asset(path)
mesh.set_material(0, unreal.load_asset('{SANDBAG_MATERIAL}'))
mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
mesh.set_editor_property('lod_group', 'LargeProp')
box = mesh.get_bounding_box()
print('PIT', mesh.get_num_triangles(0), box.min, box.max, unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False))
""", '53K sector pit mesh import')


def ue_text_vector(v):
    return f'(X={v[0]:.3f},Y={v[1]:.3f},Z={v[2]:.3f})'


def asset_text(path, kind):
    return f"/Script/Engine.{kind}'{path}.{path.split('/')[-1]}'"


def crate_transform(crate):
    """Puts the crate mesh's footprint centre on the crate spot, its narrow side towards the handler."""
    text = run_python(f"""
import unreal
box = unreal.load_asset('{CRATE_MESH}').get_bounding_box()
print('CRATEBOX', box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)
""", '53K sector pit crate bounds')
    low_x, low_y, low_z, high_x, high_y, _ = [float(v) for v in text.split('CRATEBOX ')[1].split('\\')[0].split()[:6]]
    scale = crate['scale']
    centre_x, centre_y = (low_x + high_x) * 0.5 * scale, (low_y + high_y) * 0.5 * scale
    # Turned to the handler's facing, the mesh's X runs along his line of reach: the short side must be along X.
    yaw = crate['yaw'] if high_x - low_x < high_y - low_y else crate['yaw'] + 90.0
    a = math.radians(yaw)
    offset_x = centre_x * math.cos(a) - centre_y * math.sin(a)
    offset_y = centre_x * math.sin(a) + centre_y * math.cos(a)
    x, y, _ = crate['location']
    # The bridge takes rotations as [pitch, yaw, roll].
    return [x - offset_x, y - offset_y, -low_z * scale], [0.0, yaw, 0.0]


def crew_settings(layout, carry):
    members = []
    for name in ('Crew1', 'Crew2', 'Crew3', 'Crew4'):
        member = layout[name]
        entry = f'IdleAnimation="{asset_text(member["idle"], "AnimSequence")}"'
        if member['reload']:
            entry += f',ReloadMontage="{asset_text(member["reload"], "AnimMontage")}"'
        members.append(f'({entry})')
    fire = asset_text(ROOT + '/Anims/Montage_53KPit_Fire', 'AnimMontage')
    reload_value = (f'(GunFireMontage="{fire}",CrewAnimations=({",".join(members)}),'
                    f'MinReloadTimeForLoadingSequence={MIN_RELOAD_FOR_DRILL:.6f})')
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ReloadSettings',
        value=reload_value)

    def pose(entry):
        return f'(Location={ue_text_vector(entry["location"])},Axis={ue_text_vector(entry["axis"])})'
    carry_value = (f'(RoundTag="PitRound",HandlerCrewIndex=1,LoaderCrewIndex=0,HandBone="hand_r",'
                   f'HandoverFraction={carry["handover_fraction"]:.4f},RamFraction={carry["ram_fraction"]:.4f},'
                   f'PickFraction={carry["pick_fraction"]:.4f},CrateRound={pose(carry["crate_round"])},'
                   f'IdleRound={pose(carry["idle_round"])},FirstRoundDelaySeconds=0.300000)')
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ShellCarry',
        value=carry_value)


def build_turret():
    layout = load_json('at53k_sector_crew_layout.json')
    carry = load_json('at53k_sector_carry.json')
    duplicate(PIT_TURRET, TURRET)
    for name in ('Crew1', 'Crew2', 'Crew3', 'Crew4'):
        x, y = layout[name]['location']
        act('set_component_transform', blueprint_path=TURRET, component_name=name, location=[x, y, 0.0],
            rotation=[0.0, layout[name]['yaw'], 0.0])
    act('set_static_mesh_component', blueprint_path=TURRET, component_name='ReadyCrate', mesh_path=CRATE_MESH)
    location, rotation = crate_transform(layout['crate'])
    act('set_component_transform', blueprint_path=TURRET, component_name='ReadyCrate', location=location, rotation=rotation,
        scale=[layout['crate']['scale']] * 3)
    print('ReadyCrate', location, rotation)
    crew_settings(layout, carry)
    nodes = act('get_graph_snapshot', blueprint_path=TURRET, graph_name='EventGraph', view='compact')['nodes']

    def one(predicate, label):
        matches = [n['id'] for n in nodes if predicate(n)]
        if len(matches) != 1:
            print('FAILED node lookup', label, matches)
            sys.exit(1)
        return matches[0]

    def detail(n, key):
        return n.get('detail', {}).get(key)
    init_child = one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitChildTurret', 'init child')
    init_embedded = one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitEmbeddedTurret', 'init embedded')
    for node, pin, value in ((init_child, 'NewMinPitch', f'{MIN_PITCH:.1f}'),
                             (init_embedded, 'bIsAssaultTurret', 'true'),
                             (init_embedded, 'MinTurretYaw', f'{-SECTOR_HALF_DEGREES:.1f}'),
                             (init_embedded, 'MaxTurretYaw', f'{SECTOR_HALF_DEGREES:.1f}')):
        act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('53K sector pit turret compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TURRET)
    run_python(f"""
import unreal
bp = unreal.load_asset('{TURRET}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
cdo.set_editor_property('is_fixed_emplacement', True)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print('FIXED', unreal.get_default_object(bp.generated_class()).get_editor_property('is_fixed_emplacement'))
""", '53K sector pit fixed emplacement')


def build_bxp():
    duplicate(PIT_BXP, BXP)
    nodes = act('get_graph_snapshot', blueprint_path=BXP, graph_name='EventGraph', view='compact')['nodes']
    init = [n['id'] for n in nodes if n.get('op') == 'call' and n.get('detail', {}).get('function') == 'BP_InitBuildingExp'][0]
    mesh_ref = f'{ROOT}/Meshes/{MESH}.{MESH}'
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewSmokeRadius', f'{SMOKE_RADIUS:.1f}'), ('NewAmountSmokes', str(SMOKES))):
        act('set_pin_default', blueprint_path=BXP, graph_name='EventGraph', node_id=init, pin_name=pin, value=value)
    act('set_static_mesh_component', blueprint_path=BXP, component_name='BuildingMeshComponent', mesh_path=f'{ROOT}/Meshes/{MESH}')
    act('edit_component_property', blueprint_path=BXP, component_name='Howitzer', property_name='ChildActorClass',
        value=f'{TURRET}.RTS_BP_EmbTurret_Rus53KSectorPit_C')
    # The wall is longer behind the gun than in front of it: the boxes and decal centre on the wall, not the pivot.
    centre_x = (PIT_BACK + PIT_FRONT) * 0.5
    half_x = (PIT_FRONT - PIT_BACK) * 0.5
    decal_scale = 2.0 * max(half_x, PIT_HALF_WIDTH) / DECAL_UNITS
    for component, prop, value in (
            ('bpSelectionArea', 'BoxExtent', f'(X={half_x:.1f},Y={PIT_HALF_WIDTH:.1f},Z={PIT_HEIGHT * 0.6:.1f})'),
            ('bpSelectionArea', 'RelativeLocation', f'(X={centre_x:.1f},Y=0.0,Z={PIT_HEIGHT * 0.6:.1f})'),
            ('SelectedDecal', 'RelativeLocation', f'(X={centre_x:.1f},Y=0.0,Z=10.0)'),
            ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
            ('ProgressBarWidget', 'RelativeLocation', f'(X={centre_x:.1f},Y=0.0,Z={PIT_HEIGHT + 220.0:.1f})'),
            ('NavModifier', 'FailsafeExtent', f'(X={max(-PIT_BACK, PIT_FRONT):.1f},Y={PIT_HALF_WIDTH:.1f},Z={PIT_HEIGHT:.1f})')):
        act('edit_component_property', blueprint_path=BXP, component_name=component, property_name=prop, value=value)
    compiled = act('compile_blueprint', blueprint_path=BXP)
    print('53K sector pit bxp compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=BXP)
    run_python(f"""
import unreal
bp = unreal.load_asset('{BXP}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
cdo.set_editor_property('PitExpansionType', unreal.BuildingExpansionType.{EXPANSION_TYPE})
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print('TYPE', unreal.get_default_object(bp.generated_class()).get_editor_property('PitExpansionType'))
""", '53K sector pit expansion type')


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['crew', 'mesh', 'turret', 'bxp']
    for step in steps:
        {'crew': import_crew, 'mesh': import_mesh, 'turret': build_turret, 'bxp': build_bxp}[step]()
