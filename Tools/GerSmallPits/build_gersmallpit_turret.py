"""Builds a small German pit's turret through the editor bridge (UECP direct calls, see uecp.py).

The turret is a copy of the German Pak 40 pit turret: an embedded turret with InitChildTurret / InitEmbeddedTurret, a
direct-fire projectile weapon and the gun pit crew wiring (InitPitCrew from BeginPlay, OnGunFired from
PlaySingleFireAnimation, OnReloadStarted from ReloadWeapon). On top of that:
  * the weapon on its rig with its anim blueprint (weapon, rocket crate and crew turn on the Traverse bone),
  * the infantry Panzerschreck's rocket setup: Bazooka projectile from the Muzzle socket, its launch sound, the rocket
    launch flash instead of the cannon's smoke, heavy-calibre impacts and no shell case; the weapon is
    Raketenwerfer43_88MM (Puppchen) or PanzerSchreck,
  * two German crew meshes snapped to PitOrigin (their animations carry their spots): Crew1 gunner, Crew2 loader,
  * no ready crate or rounds: the rocket and its crate are part of the gun mesh; the crew brace at every shot.

Run: python build_gersmallpit_turret.py Puppchen|Panzerschreck [--crew-only]
     (scratchpad uecp.py importable; UECP_PORT picks the editor)
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
BASE = ROOT + '/Turrets/RTS_BP_EmbTurret_GerPit_Pak40'
SOLDIER = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/GermanSoldier/Meshes/SK_WWII_GermanSoldier_varA'
CREW_ANIM_BP = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/AnimBp_B4Pit_Crew'
SCHRECK = '/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/PanzerShrek'
LAUNCH_SOUND = SCHRECK + '/Sound/S_FireWeapon_PanzeSchrek.S_FireWeapon_PanzeSchrek'
ROCKET_START = '/Game/RTS_Survival/SpecialEffects/ArmyVFX/Niagara/Projectiles/NS_Rocket_Start.NS_Rocket_Start'
HEAVY = '/Game/RTS_Survival/ImpactSfxVfx/3HeavyCalibre/'
MEDIUM_EXPLOSION = '/Game/RTS_Survival/Sounds/Explosions/General_Medium/Cue_Explosion_GeneralMedium.Cue_Explosion_GeneralMedium'
# The Panzerschreck's impacts per surface, in the Pak turret's map order (Value 0..4).
IMPACTS = [
    (HEAVY + 'Sand/NS_HeavyCaliber_SandImpact.NS_HeavyCaliber_SandImpact', MEDIUM_EXPLOSION),
    (HEAVY + 'Default_A/NS_HeavyCaliber_DefaultImpact.NS_HeavyCaliber_DefaultImpact', MEDIUM_EXPLOSION),
    (HEAVY + 'Default_A/NS_HeavyCaliber_DefaultImpact.NS_HeavyCaliber_DefaultImpact', MEDIUM_EXPLOSION),
    (HEAVY + 'Default_A/NS_HeavyCaliber_DefaultImpact.NS_HeavyCaliber_DefaultImpact', MEDIUM_EXPLOSION),
    (HEAVY + 'Stone/NS_HeavyCaliber_StoneImpact.NS_HeavyCaliber_StoneImpact',
     HEAVY + 'Stone/Cue_HeavyCaliber_StoneImpact.Cue_HeavyCaliber_StoneImpact'),
]
REMOVED_COMPONENTS = ['Crew3', 'Crew4', 'ReadyCrate', 'Round1', 'Round2']
# Below this reload time the loading drill would be rushed.
MIN_RELOAD_FOR_DRILL = 3.0
# rotation: degrees per second; pitch limits are small because the crew's hands are baked at 0 degrees pitch;
# height: the bore above the pit floor (the Puppchen stands on the 4 cm deck).
PITS = {
    'Puppchen': {'weapon': 'Raketenwerfer43_88MM', 'rotation': 40.0, 'max_pitch': 15.0, 'min_pitch': -5.0,
                 'height': 66.0, 'launch_scale': 0.8},
    'Panzerschreck': {'weapon': 'PanzerSchreck_Pit', 'rotation': 70.0, 'max_pitch': 10.0, 'min_pitch': -5.0,
                      'height': 160.0, 'launch_scale': 0.7},
}


def names(pit):
    stem = f'Ger{pit}Pit'
    return {'stem': stem, 'turret': f'{ROOT}/Turrets/RTS_BP_EmbTurret_GerPit_{pit}',
            'gun': f'{ROOT}/Meshes/{pit}/SK_{stem}_Gun', 'anim_bp': f'{ROOT}/Anims/AnimBp_{stem}'}


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
    print(text[:300])
    return text


def asset_ref(path):
    return f'"{path}.{path.split("/")[-1]}"'


def duplicate(turret):
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist('{turret}'):
    eal.delete_asset('{turret}')
print('DUP', eal.duplicate_asset('{BASE}', '{turret}'))
eal.save_asset('{turret}', only_if_is_dirty=False)
""", 'small pit duplicate turret')


def set_components(pit):
    n = names(pit)
    for name in REMOVED_COMPONENTS:
        act('delete_component', blueprint_path=n['turret'], component_name=name)
    act('set_skeletal_mesh_component', blueprint_path=n['turret'], component_name='Turret', mesh_path=n['gun'],
        anim_bp_path=n['anim_bp'])
    for name in ('Crew1', 'Crew2'):
        act('set_skeletal_mesh_component', blueprint_path=n['turret'], component_name=name, mesh_path=SOLDIER,
            anim_bp_path=CREW_ANIM_BP)
        act('set_component_transform', blueprint_path=n['turret'], component_name=name, location=[0.0, 0.0, 0.0],
            rotation=[0.0, 0.0, 0.0])


def crew_settings(pit):
    n = names(pit)
    stem = n['stem']
    crew_dir = ROOT + '/Crew'
    members = [
        f'(IdleAnimation={asset_ref(f"{crew_dir}/Anim_{stem}_Gunner_Idle")},'
        f'ReloadMontage={asset_ref(f"{crew_dir}/Montage_{stem}_Gunner_Reload")},'
        f'FireMontage={asset_ref(f"{crew_dir}/Montage_{stem}_Gunner_Fire")})',
        f'(IdleAnimation={asset_ref(f"{crew_dir}/Anim_{stem}_Loader_Idle")},'
        f'ReloadMontage={asset_ref(f"{crew_dir}/Montage_{stem}_Loader_Reload")})',
    ]
    value = (f'(GunReloadMontage={asset_ref(f"{ROOT}/Anims/Montage_{stem}_Reload")},'
             f'GunFireMontage={asset_ref(f"{ROOT}/Anims/Montage_{stem}_Fire")},'
             f'CrewAnimations=({",".join(members)}),MinReloadTimeForLoadingSequence={MIN_RELOAD_FOR_DRILL:.6f})')
    act('edit_component_property', blueprint_path=n['turret'], component_name='PitCrew', property_name='ReloadSettings',
        value=value)
    act('edit_component_property', blueprint_path=n['turret'], component_name='PitCrew', property_name='ShellCarry',
        value='(RoundTag="None")')
    for prop, prop_value in (('CrewTag', 'PitCrew'), ('CrewAttachSocket', 'PitOrigin'), ('bKeepCrewBlueprintOffset', 'false'),
                             ('CrewFireFlinchStrength', '1.0')):
        act('edit_component_property', blueprint_path=n['turret'], component_name='PitCrew', property_name=prop,
            value=prop_value)


def find_nodes(turret):
    nodes = act('get_graph_snapshot', blueprint_path=turret, graph_name='EventGraph', view='compact')['nodes']

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
        'init_embedded': one(lambda m: m.get('op') == 'call' and detail(m, 'function') == 'InitEmbeddedTurret',
                             'init embedded'),
        'weapon': one(lambda m: m.get('op') == 'make_struct' and detail(m, 'struct') == 'InitWeaponStateProjectile',
                      'weapon'),
        'vfx': one(lambda m: m.get('op') == 'make_struct' and detail(m, 'struct') == 'WeaponVFX', 'vfx'),
        'shell': one(lambda m: m.get('op') == 'make_struct' and detail(m, 'struct') == 'WeaponShellCase', 'shell'),
        'cannon_launch': one(lambda m: m.get('op') == 'call' and detail(m, 'function') == 'GetSideVFXTankLaunchCannnon',
                             'cannon launch vfx'),
        'impacts': impacts,
    }


def set_weapon(pit):
    n = names(pit)
    spec = PITS[pit]
    turret = n['turret']
    ids = find_nodes(turret)
    scale = spec['launch_scale']
    pins = [
        ('init_child', 'NewRotationSpeed', f"{spec['rotation']:.1f}"), ('init_child', 'NewMaxPitch', f"{spec['max_pitch']:.1f}"),
        ('init_child', 'NewMinPitch', f"{spec['min_pitch']:.1f}"), ('init_embedded', 'TurretHeight', f"{spec['height']:.1f}"),
        ('weapon', 'ProjectileSystem', 'Bazooka'), ('weapon', 'WeaponName', spec['weapon']),
        ('weapon', 'FireSocketName', 'Muzzle'),
        ('vfx', 'LaunchSound', LAUNCH_SOUND), ('vfx', 'LaunchScale', f'{scale},{scale},{scale}'),
        ('vfx', 'ImpactScale', '1.0,1.0,1.0'),
        ('shell', 'ShellCaseSocketName', 'Muzzle'), ('shell', 'ShellCaseVfx', ''),
    ]
    for key, pin, value in pins:
        act('set_pin_default', blueprint_path=turret, graph_name='EventGraph', node_id=ids[key], pin_name=pin, value=value)
    for index, (effect, sound) in enumerate(IMPACTS):
        node = ids['impacts'].get(index)
        if node is None:
            print('FAILED impact slot', index, ids['impacts'])
            sys.exit(1)
        for pin, value in (('ImpactEffect', effect), ('ImpactSound', sound)):
            act('set_pin_default', blueprint_path=turret, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    # The rocket's launch flash replaces the cannon's muzzle smoke.
    act('delete_nodes', blueprint_path=turret, graph_name='EventGraph', node_ids=[ids['cannon_launch']])
    act('build_blueprint_graph', blueprint_path=turret, graph_name='EventGraph',
        nodes=[{'id': 'rk_launch', 'handle': 'k2.Make LaunchEffectSettings'}],
        connections=[{'from': 'rk_launch', 'from_pin': 'LaunchEffectSettings', 'to': ids['vfx'],
                      'to_pin': 'LaunchEffectSettings'}],
        defaults=[{'node_id': 'rk_launch', 'pin_name': 'UseLaunchSettings', 'value': 'None'},
                  {'node_id': 'rk_launch', 'pin_name': 'LaunchEffect', 'value': ROCKET_START},
                  {'node_id': 'rk_launch', 'pin_name': 'bUseMuzzleLaunchSmoke', 'value': 'true'}])
    print(pit, 'weapon set:', spec['weapon'])


def build(pit, crew_only):
    n = names(pit)
    if not crew_only:
        duplicate(n['turret'])
        set_components(pit)
        set_weapon(pit)
    crew_settings(pit)
    compiled = act('compile_blueprint', blueprint_path=n['turret'])
    print(pit, 'turret compile errors', compiled.get('error_count'), compiled.get('errors'), 'warnings',
          compiled.get('warning_count'))
    act('save_asset', asset_path=n['turret'])


if __name__ == '__main__':
    pit_name = sys.argv[1] if len(sys.argv) > 1 else ''
    if pit_name not in PITS:
        print('usage: build_gersmallpit_turret.py Puppchen|Panzerschreck [--crew-only]')
        sys.exit(1)
    build(pit_name, '--crew-only' in sys.argv)
