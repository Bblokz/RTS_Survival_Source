"""Builds the Ampulomet pit turret through the editor bridge (UECP direct calls, see uecp.py).

The turret is a copy of the German leFH 18 pit turret: an embedded turret with InitChildTurret / InitEmbeddedTurret
and the gun pit crew wiring (InitPitCrew from BeginPlay, OnGunFired from PlaySingleFireAnimation, OnReloadStarted from
ReloadWeapon). On top of that:
  * the Ampulomet on its tripod with AnimBp_AmpulometPit (gun, crates and crew turn on the Traverse bone),
  * the leFH's arch projectile weapon is swapped for a direct-fire projectile weapon (SetupProjectileWeapon with the
    Bazooka projectile, like the infantry Ampulomet): the ampoule flies fast and flat at its target,
  * the weapon is Ampulomet_125MM fired from the Muzzle socket: the infantry Ampulomet's launch sound, Molotov fire
    splashes on every surface and no shell case,
  * not an artillery turret: it aims level at its target so the gunner's hands stay on the grips,
  * two Soviet crew meshes, snapped to PitOrigin (their animations carry their spots): Crew1 gunner, Crew2 loader,
  * no rounds or ready crate: the ampoule and its crates are part of the gun mesh; the crew brace at every shot.

Run: python build_ampulometpit_turret.py [--crew-only | --direct-fire-only]
     (scratchpad uecp.py importable; UECP_PORT picks the editor)
     --direct-fire-only swaps the weapon of an existing turret built with the old arch projectile and keeps the rest.
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusAmpulometPit'
BASE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Turrets/RTS_BP_EmbTurret_GerPit_LeFH18'
TURRET = ROOT + '/Turrets/RTS_BP_EmbTurret_RusAmpulometPit'
GUN_MESH = ROOT + '/Meshes/SK_AmpulometPit_Gun'
ANIM_BP = ROOT + '/Anims/AnimBp_AmpulometPit'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier'
CREW_ANIM_BP = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/AnimBp_B4Pit_Crew'
LAUNCH_SOUND = '/Game/RTS_Survival/Sounds/WeaponSounds/Rockets/SmallRocketLaunch/Cue_SmallRocketLaunch.Cue_SmallRocketLaunch'
FIRE_SPLASH = '/Game/RTS_Survival/SpecialEffects/Molotov/NS_MolotovImpact.NS_MolotovImpact'
SPLASH_SOUND = '/Game/RTS_Survival/Sounds/WeaponSounds/Rockets/PanzerwerferSmall/Cue_Fire_Panzerwerfer_Small.Cue_Fire_Panzerwerfer_Small'
SMALL_CALIBER_SOUND = '/Game/RTS_Survival/Sounds/Settings/SmallCaliber/'
REMOVED_COMPONENTS = ['Crew3', 'Crew4', 'ReadyCrate', 'Round1', 'Round2']
# (component, idle animation, reload montage, fire montage)
CREW = [('Crew1', ROOT + '/Crew/Anim_AmpulometPit_Gunner_Idle', ROOT + '/Crew/Montage_AmpulometPit_Gunner_Reload',
         ROOT + '/Crew/Montage_AmpulometPit_Gunner_Fire'),
        ('Crew2', ROOT + '/Crew/Anim_AmpulometPit_Loader_Idle', ROOT + '/Crew/Montage_AmpulometPit_Loader_Reload', None)]
# Below this reload time the breech drill would be rushed; the Ampulomet reloads in 6 s.
MIN_RELOAD_FOR_DRILL = 3.0
# A light weapon its crew swing round by hand; bore height above the pit floor.
ROTATION_SPEED = 45.0
MAX_PITCH = 20.0
MIN_PITCH = -5.0
TURRET_HEIGHT = 112.0
# The infantry Ampulomet's projectile: the Bazooka rocket's flight, fast and flat (speed is in the weapon data).
PROJECTILE_SYSTEM = 'Bazooka'
LAUNCH_SCALE = 0.6
SPLASH_SCALE = 1.6


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
""", 'Ampulomet pit duplicate turret')


def set_components():
    for name in REMOVED_COMPONENTS:
        act('delete_component', blueprint_path=TURRET, component_name=name)
    act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name='Turret', mesh_path=GUN_MESH, anim_bp_path=ANIM_BP)
    for name, _, _, _ in CREW:
        act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name=name, mesh_path=SOLDIER, anim_bp_path=CREW_ANIM_BP)
        act('set_component_transform', blueprint_path=TURRET, component_name=name, location=[0.0, 0.0, 0.0],
            rotation=[0.0, 0.0, 0.0])


def crew_settings():
    members = []
    for _, idle, reload, fire in CREW:
        entry = f'IdleAnimation={asset_ref(idle)},ReloadMontage={asset_ref(reload)}'
        if fire:
            entry += f',FireMontage={asset_ref(fire)}'
        members.append(f'({entry})')
    value = (f'(GunReloadMontage={asset_ref(ROOT + "/Anims/Montage_AmpulometPit_Reload")},'
             f'GunFireMontage={asset_ref(ROOT + "/Anims/Montage_AmpulometPit_Fire")},'
             f'CrewAnimations=({",".join(members)}),MinReloadTimeForLoadingSequence={MIN_RELOAD_FOR_DRILL:.6f})')
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ReloadSettings', value=value)
    act('edit_component_property', blueprint_path=TURRET, component_name='PitCrew', property_name='ShellCarry',
        value='(RoundTag="None")')
    for prop, prop_value in (('CrewTag', 'PitCrew'), ('CrewAttachSocket', 'PitOrigin'), ('bKeepCrewBlueprintOffset', 'false'),
                             ('CrewFireFlinchStrength', '1.0')):
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

    def every(predicate):
        return [n['id'] for n in nodes if predicate(n)]
    ids = {
        'init_child': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitChildTurret', 'init child'),
        'init_embedded': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitEmbeddedTurret', 'init embedded'),
        'anim_bp_set': one(lambda n: n.get('op') == 'prop_set' and detail(n, 'var') == 'AnimBp', 'anim bp set'),
        'init_pit_crew': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitPitCrew', 'init pit crew'),
        'vfx': one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponVFX', 'vfx'),
        'shell': one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponShellCase', 'shell'),
        'impacts': every(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'RTSSurfaceImpactData'),
        # The leFH's arch weapon block; empty once the turret fires direct.
        'arch_setup': every(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'SetupArchProjectileWeapon'),
        'arch_structs': every(lambda n: n.get('op') == 'make_struct'
                              and detail(n, 'struct') in ('InitWeaponStateArchProjectile', 'ArchProjectileSettings')),
    }
    # The self, Turret and owning player nodes that fed the arch weapon struct are reused for the direct one.
    ids['owner_feeds'] = {}
    for n in nodes:
        for pin in n.get('pins', []):
            for link in pin.get('connected_to', []):
                feeds_arch = pin.get('direction') == 'output' and link['node_id'] in ids['arch_structs']
                if feeds_arch and link['pin'] in ('WeaponOwner', 'MeshComponent', 'OwningPlayer'):
                    ids['owner_feeds'][link['pin']] = (n['id'], pin['name'])
    return ids


def make_direct_fire(ids):
    """Swaps SetupArchProjectileWeapon and its arch structs for SetupProjectileWeapon with the Bazooka projectile."""
    if not ids['arch_setup']:
        print('turret already fires direct')
        return
    feeds = ids['owner_feeds']
    if sorted(feeds) != ['MeshComponent', 'OwningPlayer', 'WeaponOwner']:
        print('FAILED owner feeds of the arch weapon', feeds)
        sys.exit(1)
    act('delete_nodes', blueprint_path=TURRET, graph_name='EventGraph', node_ids=ids['arch_setup'] + ids['arch_structs'])
    connections = [
        {'from': ids['anim_bp_set'], 'from_pin': 'then', 'to': 'amp_setup', 'to_pin': 'execute'},
        {'from': 'amp_setup', 'from_pin': 'then', 'to': ids['init_pit_crew'], 'to_pin': 'execute'},
        {'from': 'amp_weapon', 'from_pin': 'InitWeaponStateProjectile', 'to': 'amp_setup',
         'to_pin': 'ProjectileWeaponParameters'},
        {'from': ids['vfx'], 'from_pin': 'WeaponVFX', 'to': 'amp_weapon', 'to_pin': 'WeaponVFX'},
        {'from': ids['shell'], 'from_pin': 'WeaponShellCase', 'to': 'amp_weapon', 'to_pin': 'WeaponShellCase'},
    ]
    for to_pin, (node, from_pin) in feeds.items():
        connections.append({'from': node, 'from_pin': from_pin, 'to': 'amp_weapon', 'to_pin': to_pin})
    act('build_blueprint_graph', blueprint_path=TURRET, graph_name='EventGraph',
        nodes=[{'id': 'amp_setup', 'handle': 'fn.Self.SetupProjectileWeapon'},
               {'id': 'amp_weapon', 'handle': 'k2.Make InitWeaponStateProjectile'}],
        connections=connections,
        defaults=[{'node_id': 'amp_weapon', 'pin_name': 'ProjectileSystem', 'value': PROJECTILE_SYSTEM},
                  {'node_id': 'amp_weapon', 'pin_name': 'WeaponName', 'value': 'Ampulomet_125MM'},
                  {'node_id': 'amp_weapon', 'pin_name': 'FireSocketName', 'value': 'Muzzle'}])
    print('weapon swapped to a direct-fire', PROJECTILE_SYSTEM, 'projectile')


def set_weapon():
    ids = find_nodes()
    pins = [
        ('init_child', 'NewRotationSpeed', f'{ROTATION_SPEED:.1f}'), ('init_child', 'NewMaxPitch', f'{MAX_PITCH:.1f}'),
        ('init_child', 'NewMinPitch', f'{MIN_PITCH:.1f}'), ('init_embedded', 'TurretHeight', f'{TURRET_HEIGHT:.1f}'),
        ('init_embedded', 'bIsArtillery', 'false'),
        ('vfx', 'LaunchSound', LAUNCH_SOUND), ('vfx', 'LaunchScale', f'{LAUNCH_SCALE},{LAUNCH_SCALE},{LAUNCH_SCALE}'),
        ('vfx', 'ImpactScale', f'{SPLASH_SCALE},{SPLASH_SCALE},{SPLASH_SCALE}'),
        ('vfx', 'ImpactAttenuation', SMALL_CALIBER_SOUND + 'Att_SmallCaliber_WeaponImpact.Att_SmallCaliber_WeaponImpact'),
        ('vfx', 'ImpactConcurrency', SMALL_CALIBER_SOUND + 'SC_SmallCaliber_Impact.SC_SmallCaliber_Impact'),
        ('vfx', 'LaunchAttenuation', SMALL_CALIBER_SOUND + 'Att_SmallCaliber_Launch.Att_SmallCaliber_Launch'),
        ('vfx', 'LaunchConcurrency', SMALL_CALIBER_SOUND + 'SC_SmallCaliber_Launch.SC_SmallCaliber_Launch'),
        ('shell', 'ShellCaseSocketName', 'Muzzle'), ('shell', 'ShellCaseVfx', ''),
    ]
    for key, pin, value in pins:
        act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=ids[key], pin_name=pin, value=value)
    for node in ids['impacts']:
        for pin, value in (('ImpactEffect', FIRE_SPLASH), ('ImpactSound', SPLASH_SOUND)):
            act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    print('weapon set; fire splashes on', len(ids['impacts']), 'surfaces')
    make_direct_fire(ids)


def compile_and_save():
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('Ampulomet pit turret compile errors', compiled.get('error_count'), compiled.get('errors'), 'warnings',
          compiled.get('warning_count'))
    act('save_asset', asset_path=TURRET)


def build(crew_only):
    if not crew_only:
        duplicate()
        set_components()
        set_weapon()
    crew_settings()
    compile_and_save()


if __name__ == '__main__':
    if '--direct-fire-only' in sys.argv:
        make_direct_fire(find_nodes())
        compile_and_save()
    else:
        build('--crew-only' in sys.argv)
