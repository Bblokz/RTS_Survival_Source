"""Builds the German gun pit turrets through the editor bridge (UECP direct calls, see uecp.py).

Each turret is a copy of an existing embedded turret that already fires the right way: the Pak 40 pit from the
embedded Pak 38 (direct-fire projectile weapon), the leFH 18 and sFH 18 pits from the embedded leFH 105 mm (arch
artillery weapon). On top of that every turret gets:
  * the gun pit anim blueprint (whole gun turns on its root bone, the crew ride on it),
  * a UGunPitCrewComponent "PitCrew" called from BeginPlay (InitPitCrew), ReloadWeapon (OnReloadStarted) and
    PlaySingleFireAnimation (OnGunFired, which plays the gun's fire montage),
  * four German crew meshes (loader, shell handler, spotter, commander) on the crew anim blueprint,
  * the ready crate the handler fetches rounds from and the two rounds that change hands.
Crew spots are the team weapons' own crew positions; the handler's drill and the round poses come from the Blender
bake (gerpit_<pit>_carry.json).

Run: python build_gerpit_turrets.py [Pak40,LeFH18,SFH18,Flak37] [crate]
  crate: only re-place the ReadyCrate on the existing turrets (keeps later edits such as build_fire_feedback.py's)
"""
import json
import os
import sys
from uecp import call_tool

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'GunPits'))
import pit_crate

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
CREW = ROOT + '/Crew'
ANIMS = ROOT + '/Anims'
MESHES = ROOT + '/Meshes'
TURRETS = ROOT + '/Turrets'
EMBEDDED = '/Game/RTS_Survival/Blueprints/Turrets/EmbeddedTurrets/'
TW = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/'
SQUAD_ANIMS = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/TeamWeaponAnims/'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/GermanSoldier/Meshes/SK_WWII_GermanSoldier_varA'
CREW_ANIM_BP = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/AnimBp_B4Pit_Crew'
CARRY_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/Crew'
# Socket on each gun's root bone that cancels the bone's import scale and rotation (leFH root x71.7, sFH root x100,
# Pak root turned 90 degrees), so the crew, crate and rounds ride on the turning gun at 1:1 in gun space.
PIT_ORIGIN = 'PitOrigin'

PAK_CREW = [
    # (name, location, yaw, idle, reload montage); yaw -90 turns the soldier mesh to face the gun's +X.
    ('Crew1', (-148.6, 76.0, 0.0), -90.0, SQUAD_ANIMS + 'LargeMortar/AN_Infantry_MortarCrew_Crouched', CREW + '/Montage_GerPit_LoadPak'),
    ('Crew2', (0.0, 0.0, 0.0), 0.0, CREW + '/Anim_GerPitPak_Handler_Idle', CREW + '/Montage_GerPitPak_Handler_Reload'),
    ('Crew3', (-163.4, -67.5, 0.0), -90.0, SQUAD_ANIMS + 'Pak/AN_Infantry_SpotPAK_Crouched', None),
    ('Crew4', (-300.0, -150.0, 0.0), -90.0, SQUAD_ANIMS + 'Commander/AN_Infantry_CommandoCrouched', None),
]
HOWITZER_CREW = [
    ('Crew1', (-234.5, 39.7, 0.0), -90.0, SQUAD_ANIMS + 'LargeMortar/AN_Infantry_MortarCrew_Crouched', CREW + '/Montage_GerPit_LoadHowitzer'),
    ('Crew2', (0.0, 0.0, 0.0), 0.0, CREW + '/Anim_GerPitHowitzer_Handler_Idle', CREW + '/Montage_GerPitHowitzer_Handler_Reload'),
    ('Crew3', (-241.3, -54.4, 0.0), -90.0, SQUAD_ANIMS + 'Howitzer/AN_Infantry_SpotHowitzer_Standing', None),
    ('Crew4', (-430.0, -230.0, 0.0), -90.0, SQUAD_ANIMS + 'Commander/AN_Infantry_Commando', None),
]

# The Flak 37: the loader stands on the gun's platform beside the feed, the handler passes him 6-round clips from a
# crate behind; a spotter crouches behind the left leg and the commander stands at the back.
FLAK_CREW = [
    ('Crew1', (-97.92, 85.05, 0.0), -90.0, CREW + '/Anim_GerPitFlak_Loader_Idle', CREW + '/Montage_GerPit_LoadFlak'),
    ('Crew2', (0.0, 0.0, 0.0), 0.0, CREW + '/Anim_GerPitFlak_Handler_Idle', CREW + '/Montage_GerPitFlak_Handler_Reload'),
    ('Crew3', (-240.0, -90.0, 0.0), -90.0, SQUAD_ANIMS + 'Flak/AN_Infantry_Flak_CrouchedScan', None),
    ('Crew4', (-320.0, 60.0, 0.0), -90.0, SQUAD_ANIMS + 'Commander/AN_Infantry_Commando', None),
]

TURRET_SPECS = {
    'Pak40': {
        'base': EMBEDDED + 'pak38/RTS_BP_EmbTurret_Pak38',
        'mesh': TW + 'Pak40_75MM/Mesh/Sk_FullAnimPak40TW', 'root': 'B_Base',
        'carry': 'pak', 'round': MESHES + '/SM_GerPit_Shell_75', 'crew': PAK_CREW,
        'min_reload': 3.0,
        'pins': [('init_child', 'NewRotationSpeed', '22.0'), ('init_child', 'NewMaxPitch', '15.0'),
                 ('init_child', 'NewMinPitch', '-8.0'), ('init_embedded', 'TurretHeight', '96.0'),
                 ('weapon_struct', 'WeaponName', 'Pak40_3_L46_75MM_TW'), ('weapon_struct', 'FireSocketName', 'Launch'),
                 ('weapon_vfx', 'LaunchSound', '/Game/RTS_Survival/Sounds/WeaponSounds/MediumCaliber/76mmZIS/S_FireWeapon_76ZIS.S_FireWeapon_76ZIS')],
    },
    'LeFH18': {
        'base': EMBEDDED + 'LeFH_105mm/RTS_BP_EmbTurret_LeFH_105mm',
        'mesh': EMBEDDED + 'LeFH_105mm/Mesh/SK_FieldHaubitze', 'root': 'Artillery',
        'carry': 'howitzer', 'round': MESHES + '/SM_GerPit_Shell_105', 'crew': HOWITZER_CREW,
        'min_reload': 3.0,
        'pins': [('init_child', 'NewRotationSpeed', '18.0'), ('init_child', 'NewMaxPitch', '42.0'),
                 ('init_embedded', 'TurretHeight', '119.0'),
                 ('weapon_struct', 'WeaponName', 'LeFH_18_105MM_TW')],
    },
    'SFH18': {
        'base': EMBEDDED + 'LeFH_105mm/RTS_BP_EmbTurret_LeFH_105mm',
        'mesh': TW + 'LFH18/Mesh/SK_Ger_SFH18Anim', 'root': '37mmFlak',
        'carry': 'howitzer', 'round': MESHES + '/SM_GerPit_Shell_150', 'crew': HOWITZER_CREW,
        'min_reload': 6.0,
        'pins': [('init_child', 'NewRotationSpeed', '12.0'), ('init_child', 'NewMaxPitch', '45.0'),
                 ('init_embedded', 'TurretHeight', '143.0'),
                 ('weapon_struct', 'WeaponName', 'sFH18_150MM_TW'),
                 ('weapon_vfx', 'LaunchSound', '/Game/RTS_Survival/Sounds/WeaponSounds/HeavyCaliber/Brummbar/S_BrummbarFire.S_BrummbarFire')],
    },
    'Flak37': {
        'base': EMBEDDED + '37mmFlak/RTS_BP_EmbTurret_37mmFlak',
        'mesh': TW + 'Flak37mm/Mesh/SK_TW_Flak37MM', 'root': '37mmFlak',
        'carry': 'flak', 'round': MESHES + '/SM_GerPit_Clip_37', 'crew': FLAK_CREW,
        'min_reload': 3.0,
        # Fast anti-aircraft traverse; the gun fires from the team weapon mesh's own sockets.
        'pins': [('init_child', 'NewRotationSpeed', '45.0'), ('init_child', 'NewMaxPitch', '85.0'),
                 ('init_child', 'NewMinPitch', '-5.0'), ('init_embedded', 'TurretHeight', '158.0'),
                 ('weapon_struct', 'WeaponName', 'Flak36_37MM_Pit'), ('weapon_struct', 'FireSocketName', 'Launch'),
                 ('shell_case', 'ShellCaseSocketName', 'Out')],
    },
}


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text, strict=False)
    if not data.get('ok', True):
        print('FAILED', action, json.dumps(params)[:300], '->', text[:900])
        sys.exit(1)
    return data


def run_python(code, description):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': description}})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '')
    print(text[:500])
    return text


def find_nodes(bp):
    """Finds the nodes to patch by role; duplicating a blueprint gives its nodes new ids."""
    nodes = act('get_graph_snapshot', blueprint_path=bp, graph_name='EventGraph', view='compact')['nodes']

    def one(predicate, label):
        matches = [n for n in nodes if predicate(n)]
        if len(matches) != 1:
            print('FAILED node lookup', label, [m['id'] for m in matches])
            sys.exit(1)
        return matches[0]

    def detail(n, key):
        return n.get('detail', {}).get(key)
    montage = one(lambda n: n.get('op') == 'play_montage', 'fire montage')
    turret_get = None
    for pin in montage.get('pins', []):
        for link in pin.get('connected_to', []):
            linked = next((n for n in nodes if n['id'] == link['node_id']), None)
            if linked and linked.get('op') == 'prop_get' and detail(linked, 'var') == 'Turret':
                turret_get = linked['id']
    ids = {
        'fire_event': one(lambda n: n.get('op') == 'event' and detail(n, 'name') == 'PlaySingleFireAnimation', 'fire event')['id'],
        'fire_montage': montage['id'],
        'chain_end': one(lambda n: n.get('op') == 'call' and detail(n, 'function') in ('SetupProjectileWeapon', 'SetupArchProjectileWeapon'), 'weapon setup')['id'],
        'init_child': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitChildTurret', 'init child')['id'],
        'init_embedded': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitEmbeddedTurret', 'init embedded')['id'],
        'weapon_struct': one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') in ('InitWeaponStateProjectile', 'InitWeaponStateArchProjectile'), 'weapon struct')['id'],
        'weapon_vfx': one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponVFX', 'weapon vfx')['id'],
    }
    ids['fire_turret_get'] = turret_get
    shell_cases = [n['id'] for n in nodes if n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponShellCase']
    ids['shell_case'] = shell_cases[0] if len(shell_cases) == 1 else None
    return ids


def duplicate(spec, gun):
    path = f'{TURRETS}/RTS_BP_EmbTurret_GerPit_{gun}'
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{path}'):
    eal.delete_asset('{path}')
print('DUP', eal.duplicate_asset('{spec['base']}', '{path}'))
eal.save_asset('{path}', only_if_is_dirty=False)
""", 'GerPit duplicate turret')
    return path


def ue_text_vector(v):
    return f'(X={v[0]:.3f},Y={v[1]:.3f},Z={v[2]:.3f})'


def place_crate(bp, carry):
    """Puts the ready crate under the round the handler picks up (the drill's crate spot)."""
    location, rotation, scale = pit_crate.crate_transform(pit_crate.CRATE_MESH, carry['crate'])
    act('set_static_mesh_component', blueprint_path=bp, component_name='ReadyCrate', mesh_path=pit_crate.CRATE_MESH)
    act('set_component_transform', blueprint_path=bp, component_name='ReadyCrate', location=location, rotation=rotation,
        scale=[scale] * 3)
    print(bp.split('/')[-1], 'ReadyCrate', location, rotation, scale)


def add_components(bp, spec, carry):
    act('set_skeletal_mesh_component', blueprint_path=bp, component_name='Turret', mesh_path=spec['mesh'],
        anim_bp_path=f"{ANIMS}/AnimBp_GerPit_{spec['gun']}")
    act('add_component', blueprint_path=bp, component_class='GunPitCrewComponent', component_name='PitCrew')
    for name, location, yaw, _, _ in spec['crew']:
        act('add_component', blueprint_path=bp, component_class='SkeletalMeshComponent', component_name=name)
        act('set_skeletal_mesh_component', blueprint_path=bp, component_name=name, mesh_path=SOLDIER, anim_bp_path=CREW_ANIM_BP)
        act('set_component_transform', blueprint_path=bp, component_name=name, location=list(location), rotation=[0.0, yaw, 0.0])
        act('edit_component_property', blueprint_path=bp, component_name=name, property_name='ComponentTags', value='("PitCrew")')
    act('add_component', blueprint_path=bp, component_class='StaticMeshComponent', component_name='ReadyCrate')
    place_crate(bp, carry)
    act('edit_component_property', blueprint_path=bp, component_name='ReadyCrate', property_name='ComponentTags',
        value=f'("PitSocket:{PIT_ORIGIN}")')
    for name in ('Round1', 'Round2'):
        act('add_component', blueprint_path=bp, component_class='StaticMeshComponent', component_name=name)
        act('set_static_mesh_component', blueprint_path=bp, component_name=name, mesh_path=spec['round'])
        act('edit_component_property', blueprint_path=bp, component_name=name, property_name='ComponentTags', value='("PitRound")')
    for name in ('ReadyCrate', 'Round1', 'Round2'):
        # Props riding on the gun must not push the crew or block the turret's own traces.
        result = call_tool('call_action', {'action': 'edit_component_property', 'params': {
            'blueprint_path': bp, 'component_name': name, 'property_name': 'BodyInstance',
            'value': '(CollisionEnabled=NoCollision,CollisionProfileName="NoCollision")'}})
        print(name, 'collision', result.get('result', {}).get('content', [{}])[0].get('text', '')[:160])


def crew_settings(bp, spec, carry):
    members = []
    for _, _, _, idle, reload in spec['crew']:
        idle_name = idle.split('/')[-1]
        entry = f'IdleAnimation="{idle}.{idle_name}"'
        if reload:
            entry += f',ReloadMontage="{reload}.{reload.split("/")[-1]}"'
        members.append(f'({entry})')
    fire = f"{ANIMS}/Montage_GerPit_{spec['gun']}_Fire"
    reload_value = (f'(GunFireMontage="{fire}.Montage_GerPit_{spec["gun"]}_Fire",CrewAnimations=({",".join(members)}),'
                    f'MinReloadTimeForLoadingSequence={spec["min_reload"]:.6f})')
    act('edit_component_property', blueprint_path=bp, component_name='PitCrew', property_name='ReloadSettings', value=reload_value)

    def pose(entry):
        return f'(Location={ue_text_vector(entry["location"])},Axis={ue_text_vector(entry["axis"])})'
    carry_value = (f'(RoundTag="PitRound",HandlerCrewIndex=1,LoaderCrewIndex=0,HandBone="hand_r",'
                   f'HandoverFraction={carry["handover_fraction"]:.4f},RamFraction={carry["ram_fraction"]:.4f},'
                   f'PickFraction={carry["pick_fraction"]:.4f},CrateRound={pose(carry["crate_round"])},'
                   f'IdleRound={pose(carry["idle_round"])},FirstRoundDelaySeconds=0.300000)')
    act('edit_component_property', blueprint_path=bp, component_name='PitCrew', property_name='ShellCarry', value=carry_value)
    for prop, value in (('CrewTag', 'PitCrew'), ('PropSocketTagPrefix', 'PitSocket:'), ('CrewAttachSocket', PIT_ORIGIN),
                        ('bKeepCrewBlueprintOffset', 'true')):
        act('edit_component_property', blueprint_path=bp, component_name='PitCrew', property_name=prop, value=value)


def rewire_graph(bp, spec):
    ids = find_nodes(bp)
    for key, pin, value in spec['pins']:
        act('set_pin_default', blueprint_path=bp, graph_name='EventGraph', node_id=ids[key], pin_name=pin, value=value)
    # The gun's own fire montage goes through the crew component, which knows when a drill runs.
    act('delete_nodes', blueprint_path=bp, graph_name='EventGraph',
        node_ids=[i for i in (ids['fire_montage'], ids['fire_turret_get']) if i])
    act('build_blueprint_graph', blueprint_path=bp, graph_name='EventGraph',
        nodes=[
            {'id': 'gp_pc_init', 'handle': 'var.get.PitCrew'},
            {'id': 'gp_turret', 'handle': 'var.get.Turret'},
            {'id': 'gp_init', 'handle': 'fn.GunPitCrewComponent.InitPitCrew'},
            {'id': 'gp_pc_fire', 'handle': 'var.get.PitCrew'},
            {'id': 'gp_fired', 'handle': 'fn.GunPitCrewComponent.OnGunFired'},
            {'id': 'gp_reload_ev', 'handle': 'ev.ReloadWeapon'},
            {'id': 'gp_pc_reload', 'handle': 'var.get.PitCrew'},
            {'id': 'gp_reload', 'handle': 'fn.GunPitCrewComponent.OnReloadStarted'},
        ],
        connections=[
            {'from': ids['chain_end'], 'from_pin': 'then', 'to': 'gp_init', 'to_pin': 'execute'},
            {'from': 'gp_pc_init', 'from_pin': 'PitCrew', 'to': 'gp_init', 'to_pin': 'self'},
            {'from': 'gp_turret', 'from_pin': 'Turret', 'to': 'gp_init', 'to_pin': 'GunMesh'},
            {'from': ids['fire_event'], 'from_pin': 'then', 'to': 'gp_fired', 'to_pin': 'execute'},
            {'from': 'gp_pc_fire', 'from_pin': 'PitCrew', 'to': 'gp_fired', 'to_pin': 'self'},
            {'from': 'gp_reload_ev', 'from_pin': 'then', 'to': 'gp_reload', 'to_pin': 'execute'},
            {'from': 'gp_pc_reload', 'from_pin': 'PitCrew', 'to': 'gp_reload', 'to_pin': 'self'},
            {'from': 'gp_reload_ev', 'from_pin': 'ReloadTime', 'to': 'gp_reload', 'to_pin': 'ReloadTime'},
        ])


def load_carry(spec):
    with open(f"{CARRY_DIR}/gerpit_{spec['carry']}_carry.json") as handle:
        return json.load(handle)


def compile_and_save(bp, gun):
    compiled = act('compile_blueprint', blueprint_path=bp)
    print(gun, 'compile errors', compiled.get('error_count'), compiled.get('errors'), 'warnings', compiled.get('warning_count'))
    act('save_asset', asset_path=bp)


def replace_crate(gun):
    bp = f'{TURRETS}/RTS_BP_EmbTurret_GerPit_{gun}'
    place_crate(bp, load_carry(TURRET_SPECS[gun]))
    compile_and_save(bp, gun)


def build(gun):
    spec = dict(TURRET_SPECS[gun], gun=gun)
    carry = load_carry(spec)
    bp = duplicate(spec, gun)
    add_components(bp, spec, carry)
    crew_settings(bp, spec, carry)
    rewire_graph(bp, spec)
    compile_and_save(bp, gun)


if __name__ == '__main__':
    guns = sys.argv[1].split(',') if len(sys.argv) > 1 else list(TURRET_SPECS)
    crate_only = len(sys.argv) > 2 and sys.argv[2] == 'crate'
    for gun_name in guns:
        if crate_only:
            replace_crate(gun_name)
        else:
            build(gun_name)
