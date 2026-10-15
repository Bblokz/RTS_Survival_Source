"""Builds the DShK pit turret through the editor bridge (UECP direct calls, see uecp.py).

The turret is a copy of the German Pak 40 pit turret (embedded turret with InitChildTurret / InitEmbeddedTurret and
the gun pit crew wiring: InitPitCrew from BeginPlay, OnGunFired from PlaySingleFireAnimation, OnReloadStarted from
ReloadWeapon). On top of that:
  * the DShK on its tripod with AnimBp_DShKPit (gun and crew traverse on the Traverse bone, the tripod stays),
  * the Pak's projectile weapon is replaced by the DShK trace weapon in random bursts, set up like the MG34 top
    turret (MG launch VFX, small-arms impacts, casings),
  * two Soviet crew meshes, snapped to PitOrigin (their animations carry their spots): Crew1 gunner, Crew2 loader,
  * no rounds or ready crate: the loader's spare boxes are part of the gun mesh.
--with-fire also sets the gunner's FireMontage (needs an editor built with FGunPitCrewMemberAnimations::FireMontage).

Run: python build_dshkpit_turret.py [--with-fire] [--crew-only]
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit'
BASE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Turrets/RTS_BP_EmbTurret_GerPit_Pak40'
TURRET = ROOT + '/Turrets/RTS_BP_EmbTurret_RusDShKPit'
GUN_MESH = ROOT + '/Meshes/SK_DShKPit_Gun'
ANIM_BP = ROOT + '/Anims/AnimBp_DShKPit'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier'
CREW_ANIM_BP = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/AnimBp_B4Pit_Crew'
DSHK_SOUND = '/Game/RTS_Survival/Blueprints/Turrets/Russian/HullWeapons/Dshk_FX/S_FireWeapon_Dshk.S_FireWeapon_Dshk'
CASINGS = '/Game/RTS_Survival/Blueprints/Turrets/German/Light/PumaTurrets/50mmPuma/VFX/NS_Casings_MgPuma.NS_Casings_MgPuma'
BOUNCE_SOUND = '/Game/RTS_Survival/ImpactSfxVfx/0Bulllet/Bounce/Cue_SmallArms_Bounce.Cue_SmallArms_Bounce'
BOUNCE_EFFECT = '/Game/RTS_Survival/ImpactSfxVfx/0Bulllet/Bounce/NS_SmallArms_Bounce1.NS_SmallArms_Bounce1'
SMALL_ARMS_SOUND = '/Game/RTS_Survival/Sounds/Settings/SmallArms/'
REMOVED_COMPONENTS = ['Crew3', 'Crew4', 'ReadyCrate', 'Round1', 'Round2']
# (component, idle animation, reload montage, fire montage)
CREW = [('Crew1', ROOT + '/Crew/Anim_DShKPit_Gunner_Idle', ROOT + '/Crew/Montage_DShKPit_Gunner_Reload',
         ROOT + '/Crew/Montage_DShKPit_Gunner_Fire'),
        ('Crew2', ROOT + '/Crew/Anim_DShKPit_Loader_Idle', ROOT + '/Crew/Montage_DShKPit_Loader_Reload', None)]
# Below this reload time the box swap would be rushed; the DShK reloads in 6 s.
MIN_RELOAD_FOR_DRILL = 3.0
# Turret: an MG on a pintle swings fast; bore height above the pit floor.
ROTATION_SPEED = 70.0
MAX_PITCH = 25.0
MIN_PITCH = -10.0
TURRET_HEIGHT = 108.0
BURST_COOLDOWN = 0.12
MIN_BURST = 6
MAX_BURST = 14


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
    print(text[:400])
    return text


def asset_ref(path):
    return f'"{path}.{path.split("/")[-1]}"'


def duplicate():
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{TURRET}'):
    eal.delete_asset('{TURRET}')
print('DUP', eal.duplicate_asset('{BASE}', '{TURRET}'))
eal.save_asset('{TURRET}', only_if_is_dirty=False)
""", 'DShK pit duplicate turret')


def set_components():
    for name in REMOVED_COMPONENTS:
        act('delete_component', blueprint_path=TURRET, component_name=name)
    act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name='Turret', mesh_path=GUN_MESH, anim_bp_path=ANIM_BP)
    for name, _, _, _ in CREW:
        act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name=name, mesh_path=SOLDIER, anim_bp_path=CREW_ANIM_BP)
        act('set_component_transform', blueprint_path=TURRET, component_name=name, location=[0.0, 0.0, 0.0],
            rotation=[0.0, 0.0, 0.0])


def crew_settings(with_fire):
    members = []
    for _, idle, reload, fire in CREW:
        entry = f'IdleAnimation={asset_ref(idle)},ReloadMontage={asset_ref(reload)}'
        if with_fire and fire:
            entry += f',FireMontage={asset_ref(fire)}'
        members.append(f'({entry})')
    value = (f'(GunReloadMontage={asset_ref(ROOT + "/Anims/Montage_DShKPit_Reload")},'
             f'GunFireMontage={asset_ref(ROOT + "/Anims/Montage_DShKPit_Fire")},'
             f'CrewAnimations=({",".join(members)}),MinReloadTimeForLoadingSequence={MIN_RELOAD_FOR_DRILL:.6f})')
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ReloadSettings', value=value)
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ShellCarry',
        value='(RoundTag="None")')
    for prop, prop_value in (('CrewTag', 'PitCrew'), ('CrewAttachSocket', 'PitOrigin'), ('bKeepCrewBlueprintOffset', 'false')):
        act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name=prop, value=prop_value)


def find_nodes():
    nodes = act('get_graph_snapshot', blueprint_path=TURRET, graph_name='EventGraph', view='compact')['nodes']

    def detail(n, key):
        return n.get('detail', {}).get(key)

    def one(predicate, label):
        matches = [n for n in nodes if predicate(n)]
        if len(matches) != 1:
            print('FAILED node lookup', label, [m['id'] for m in matches])
            sys.exit(1)
        return matches[0]['id']
    ids = {
        'init_child': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitChildTurret', 'init child'),
        'init_embedded': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitEmbeddedTurret', 'init embedded'),
        'anim_bp_set': one(lambda n: n.get('op') == 'prop_set' and detail(n, 'var') == 'AnimBp', 'anim bp set'),
        'old_setup': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'SetupProjectileWeapon', 'old setup'),
        'init_pit_crew': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitPitCrew', 'init pit crew'),
    }
    # The Pak's cannon weapon block: projectile struct and everything that only feeds its VFX and shell case.
    cannon_structs = ('InitWeaponStateProjectile', 'WeaponVFX', 'WeaponShellCase', 'RTSSurfaceImpactData')
    ids['old_weapon'] = [n['id'] for n in nodes if (n.get('op') == 'make_struct' and detail(n, 'struct') in cannon_structs)
                         or (n.get('op') == 'opaque' and detail(n, 'summary') == 'Make Map')
                         or (n.get('op') == 'call' and detail(n, 'function') in ('GetHeavyVfxOverwriteData',
                                                                               'GetSideVFXTankLaunchCannnon'))]
    owner_feeds = []
    for n in nodes:
        for pin in n.get('pins', []):
            for link in pin.get('connected_to', []):
                if pin.get('direction') == 'output' and link['pin'] in ('WeaponOwner', 'MeshComponent', 'OwningPlayer'):
                    owner_feeds.append(n['id'])
    ids['old_weapon'] += owner_feeds
    return ids


def rewire_graph():
    ids = find_nodes()
    for key, pin, value in (('init_child', 'NewRotationSpeed', ROTATION_SPEED), ('init_child', 'NewMaxPitch', MAX_PITCH),
                            ('init_child', 'NewMinPitch', MIN_PITCH), ('init_embedded', 'TurretHeight', TURRET_HEIGHT)):
        act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=ids[key], pin_name=pin, value=f'{value:.1f}')
    act('delete_nodes', blueprint_path=TURRET, graph_name='EventGraph', node_ids=[ids['old_setup']] + ids['old_weapon'])
    act('build_blueprint_graph', blueprint_path=TURRET, graph_name='EventGraph',
        nodes=[
            {'id': 'dk_setup', 'handle': 'fn.Self.SetupTraceWeapon'},
            {'id': 'dk_trace', 'handle': 'k2.Make Init Weapon Stat Trace'},
            {'id': 'dk_vfx', 'handle': 'k2.Make Weapon VFX'},
            {'id': 'dk_shell', 'handle': 'k2.Make Weapon Shell Case'},
            {'id': 'dk_launch', 'handle': 'fn.BPF_ImpactHelpers.GetGerTankMgLaunchVfx'},
            {'id': 'dk_impacts', 'handle': 'fn.BPF_ImpactHelpers.GetSmallArmsStandardImpact'},
            {'id': 'dk_self', 'handle': 'k2.Self'},
            {'id': 'dk_turret', 'handle': 'var.get.Turret'},
            {'id': 'dk_owner', 'handle': 'fn.Self.GetOwningPlayerForWeaponInit'},
        ],
        connections=[
            {'from': ids['anim_bp_set'], 'from_pin': 'then', 'to': 'dk_setup', 'to_pin': 'execute'},
            {'from': 'dk_setup', 'from_pin': 'then', 'to': ids['init_pit_crew'], 'to_pin': 'execute'},
            {'from': 'dk_trace', 'from_pin': 'InitWeaponStatTrace', 'to': 'dk_setup', 'to_pin': 'TraceWeaponParameters'},
            {'from': 'dk_vfx', 'from_pin': 'WeaponVFX', 'to': 'dk_trace', 'to_pin': 'WeaponVFX'},
            {'from': 'dk_shell', 'from_pin': 'WeaponShellCase', 'to': 'dk_trace', 'to_pin': 'WeaponShellCase'},
            {'from': 'dk_launch', 'from_pin': 'MgLaunch', 'to': 'dk_vfx', 'to_pin': 'LaunchEffectSettings'},
            {'from': 'dk_impacts', 'from_pin': 'SmallArmsImpactsPerSurface', 'to': 'dk_vfx', 'to_pin': 'SurfaceImpactEffects'},
            {'from': 'dk_self', 'from_pin': 'self', 'to': 'dk_trace', 'to_pin': 'WeaponOwner'},
            {'from': 'dk_turret', 'from_pin': 'Turret', 'to': 'dk_trace', 'to_pin': 'MeshComponent'},
            {'from': 'dk_owner', 'from_pin': 'ReturnValue', 'to': 'dk_trace', 'to_pin': 'OwningPlayer'},
        ],
        defaults=[
            {'node_id': 'dk_trace', 'pin_name': 'WeaponName', 'value': 'DShk_12_7MM'},
            {'node_id': 'dk_trace', 'pin_name': 'FireSocketName', 'value': 'Muzzle'},
            {'node_id': 'dk_trace', 'pin_name': 'TraceProjectileType', 'value': 1},
            {'node_id': 'dk_trace', 'pin_name': 'WeaponBurstMode', 'value': 'RandomBurst'},
            {'node_id': 'dk_trace', 'pin_name': 'BurstCooldown', 'value': BURST_COOLDOWN},
            {'node_id': 'dk_trace', 'pin_name': 'MinBurstAmount', 'value': MIN_BURST},
            {'node_id': 'dk_trace', 'pin_name': 'SingleBurstAmountMaxBurstAmount', 'value': MAX_BURST},
            {'node_id': 'dk_vfx', 'pin_name': 'LaunchSound', 'value': DSHK_SOUND},
            {'node_id': 'dk_vfx', 'pin_name': 'BounceSound', 'value': BOUNCE_SOUND},
            {'node_id': 'dk_vfx', 'pin_name': 'BounceEffect', 'value': BOUNCE_EFFECT},
            {'node_id': 'dk_vfx', 'pin_name': 'ImpactAttenuation', 'value': SMALL_ARMS_SOUND + 'Att_SmallArms_WeaponImpact.Att_SmallArms_WeaponImpact'},
            {'node_id': 'dk_vfx', 'pin_name': 'ImpactConcurrency', 'value': SMALL_ARMS_SOUND + 'SC_SmallArms_Impact.SC_SmallArms_Impact'},
            {'node_id': 'dk_vfx', 'pin_name': 'LaunchAttenuation', 'value': SMALL_ARMS_SOUND + 'Att_SmallArms_Launch.Att_SmallArms_Launch'},
            {'node_id': 'dk_vfx', 'pin_name': 'LaunchConcurrency', 'value': SMALL_ARMS_SOUND + 'SC_SmallArms_Launch.SC_SmallArms_Launch'},
            {'node_id': 'dk_shell', 'pin_name': 'ShellCaseSocketName', 'value': 'Muzzle'},
            {'node_id': 'dk_shell', 'pin_name': 'ShellCaseVfx', 'value': CASINGS},
            {'node_id': 'dk_shell', 'pin_name': 'MeshSizeMlt', 'value': 0.7},
            {'node_id': 'dk_shell', 'pin_name': 'SpriteSizeMlt', 'value': 0.7},
        ])


def build(with_fire, crew_only):
    if not crew_only:
        duplicate()
        set_components()
        rewire_graph()
    crew_settings(with_fire)
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('DShK pit turret compile errors', compiled.get('error_count'), compiled.get('errors'), 'warnings',
          compiled.get('warning_count'))
    act('save_asset', asset_path=TURRET)


if __name__ == '__main__':
    build('--with-fire' in sys.argv, '--crew-only' in sys.argv)
