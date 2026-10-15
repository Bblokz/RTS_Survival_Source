"""Builds the Ampulomet pit turret through the editor bridge (UECP direct calls, see uecp.py).

The turret is a copy of the German leFH 18 pit turret: an embedded turret with InitChildTurret / InitEmbeddedTurret,
an arch projectile weapon (the ampoule is lobbed) and the gun pit crew wiring (InitPitCrew from BeginPlay, OnGunFired
from PlaySingleFireAnimation, OnReloadStarted from ReloadWeapon). On top of that:
  * the Ampulomet on its tripod with AnimBp_AmpulometPit (gun, crates and crew turn on the Traverse bone),
  * the weapon is Ampulomet_125MM fired from the Muzzle socket: a low lob without the artillery descent whistle,
    the infantry Ampulomet's launch sound, Molotov fire splashes on every surface and no shell case,
  * not an artillery turret: it aims level at its target so the gunner's hands stay on the grips (the arc is the
    projectile's own),
  * two Soviet crew meshes, snapped to PitOrigin (their animations carry their spots): Crew1 gunner, Crew2 loader,
  * no rounds or ready crate: the ampoule and its crates are part of the gun mesh; the crew brace at every shot.

Run: python build_ampulometpit_turret.py [--crew-only]   (scratchpad uecp.py importable; UECP_PORT picks the editor)
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
# A low lob: the ampoule flies flatter than a howitzer shell.
APEX_HEIGHT_MULTIPLIER = 0.25
APEX_HEIGHT_OFFSET = 250.0
MIN_APEX_OFFSET = -200.0
CURVATURE_MULTIPLIER = 1.5
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
    return {
        'init_child': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitChildTurret', 'init child'),
        'init_embedded': one(lambda n: n.get('op') == 'call' and detail(n, 'function') == 'InitEmbeddedTurret', 'init embedded'),
        'weapon': one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'InitWeaponStateArchProjectile', 'weapon'),
        'arch': one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'ArchProjectileSettings', 'arch'),
        'vfx': one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponVFX', 'vfx'),
        'shell': one(lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponShellCase', 'shell'),
        'impacts': [n['id'] for n in nodes if n.get('op') == 'make_struct' and detail(n, 'struct') == 'RTSSurfaceImpactData'],
    }


def set_weapon():
    ids = find_nodes()
    pins = [
        ('init_child', 'NewRotationSpeed', f'{ROTATION_SPEED:.1f}'), ('init_child', 'NewMaxPitch', f'{MAX_PITCH:.1f}'),
        ('init_child', 'NewMinPitch', f'{MIN_PITCH:.1f}'), ('init_embedded', 'TurretHeight', f'{TURRET_HEIGHT:.1f}'),
        ('init_embedded', 'bIsArtillery', 'false'),
        ('weapon', 'WeaponName', 'Ampulomet_125MM'), ('weapon', 'FireSocketName', 'Muzzle'),
        ('arch', 'ApexHeightMultiplier', f'{APEX_HEIGHT_MULTIPLIER:.3f}'), ('arch', 'ApexHeightOffset', f'{APEX_HEIGHT_OFFSET:.1f}'),
        ('arch', 'MinApexOffset', f'{MIN_APEX_OFFSET:.1f}'), ('arch', 'CurvatureVerticalVelocityMultiplier', f'{CURVATURE_MULTIPLIER:.2f}'),
        ('arch', 'DescentSound', ''),
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


def build(crew_only):
    if not crew_only:
        duplicate()
        set_components()
        set_weapon()
    crew_settings()
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('Ampulomet pit turret compile errors', compiled.get('error_count'), compiled.get('errors'), 'warnings',
          compiled.get('warning_count'))
    act('save_asset', asset_path=TURRET)


if __name__ == '__main__':
    build('--crew-only' in sys.argv)
