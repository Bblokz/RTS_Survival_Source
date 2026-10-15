"""Builds the 53-K team weapon, its squad, its wreck and the spawner entry through the editor bridge.

Copies of the ZiS-3's (same skeleton, same kind of weapon) with the 53-K's assets:
  DestroyedTW_53K           the scavengeable wreck, SM_Destroyed_53K
  Rus_TW_53K_45mm           the gun: SK_TW_53K with AnimBp_TW_53K, weapon M53K_45MM_TW fired from Launch53K (casings
                            from Out53K), the 45 mm T-26 gun's sound, crew spots moved to the smaller gun, turret
                            height at the 53-K's bore, spawns DestroyedTW_53K when destroyed
  Rus_TWSquad_53K_45mm      the squad: the ZiS-3's Soviet crew with TeamWeaponClass = Rus_TW_53K_45mm
  RTS_BP_RTSAsyncSpawner    SetupRusSquads row Squad_Rus_53K_45MM -> Rus_TWSquad_53K_45mm
The crew animations need no setup: the squad unit anim blueprint's crew montages fall back to their base entries
(the anti-tank gun loader and spotter) for a subtype without an override.

Run: python build_53k_teamweapon.py
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/53K_45MM'
ZIS = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/Zis_76MM'
WRECK = ROOT + '/DestroyedTW_53K'
TEAM_WEAPON = ROOT + '/Rus_TW_53K_45mm'
SQUAD = ROOT + '/Rus_TWSquad_53K_45mm'
SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
LAUNCH_SOUND = '/Game/RTS_Survival/Sounds/WeaponSounds/MediumCaliber/45mmT26/S_FireWeapon_45mmT26.S_FireWeapon_45mmT26'
# Crew spots (Unreal gun frame): the 53-K's breech is at x -72; the loader stands beside it, between the open
# trails, the spotter further back on the other side.
CREW_SPOTS = {'Loader': (-150.0, 72.0), 'Spotter': (-210.0, -82.0)}
CREW_SPOT_HEIGHT = 21.165
BORE_HEIGHT = 76.0
LAUNCH_SCALE = 0.85
CASING_SCALE = 0.6


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


def duplicate(source, target):
    # An existing copy is reused: deleting a loaded Blueprint asks for confirmation, which a windowless editor
    # cannot show (it dies with a fatal window error). Every later step sets its values again.
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if not eal.does_asset_exist('{target}'):
    print('DUP', eal.duplicate_asset('{source}', '{target}'))
    eal.save_asset('{target}', only_if_is_dirty=False)
""", '53K duplicate ' + target.split('/')[-1])


def nodes_of(bp):
    return act('get_graph_snapshot', blueprint_path=bp, graph_name='EventGraph', view='compact')['nodes']


def one_node(nodes, predicate, label):
    matches = [n['id'] for n in nodes if predicate(n)]
    if len(matches) != 1:
        print('FAILED node lookup', label, matches)
        sys.exit(1)
    return matches[0]


def build_wreck():
    duplicate(ZIS + '/DestroyedTW_Zis3_76', WRECK)
    act('set_static_mesh_component', blueprint_path=WRECK, component_name='Weapon', mesh_path=ROOT + '/Mesh/SM_Destroyed_53K')
    compiled = act('compile_blueprint', blueprint_path=WRECK)
    print('wreck compile errors', compiled.get('error_count'))
    act('save_asset', asset_path=WRECK)


def build_team_weapon():
    duplicate(ZIS + '/Rus_TW_Zis3_76', TEAM_WEAPON)
    # The bridge's mesh + anim BP check compares skeleton paths in two spellings and refuses this valid pair, so the
    # anim class is set as a plain component property.
    act('set_skeletal_mesh_component', blueprint_path=TEAM_WEAPON, component_name='WeaponMesh',
        mesh_path=ROOT + '/Mesh/SK_TW_53K')
    act('edit_component_property', blueprint_path=TEAM_WEAPON, component_name='WeaponMesh', property_name='AnimClass',
        value=f"{ROOT}/AnimBp_TW_53K.AnimBp_TW_53K_C")
    for name, (x, y) in CREW_SPOTS.items():
        act('set_component_transform', blueprint_path=TEAM_WEAPON, component_name=name, location=[x, y, CREW_SPOT_HEIGHT])
    act('set_blueprint_variable_default', blueprint_path=TEAM_WEAPON, variable_name='Weapon Name', default_value='M53K_45MM_TW')
    nodes = nodes_of(TEAM_WEAPON)

    def struct(name):
        return lambda n: n.get('op') == 'make_struct' and n.get('detail', {}).get('struct') == name
    weapon = one_node(nodes, struct('InitWeaponStateProjectile'), 'weapon struct')
    shell = one_node(nodes, struct('WeaponShellCase'), 'shell case')
    vfx = one_node(nodes, struct('WeaponVFX'), 'weapon vfx')
    wreck = one_node(nodes, struct('DestroySpawnActor'), 'wreck spawn')
    for node, pin, value in ((weapon, 'FireSocketName', 'Launch53K'), (shell, 'ShellCaseSocketName', 'Out53K'),
                             (shell, 'MeshSizeMlt', f'{CASING_SCALE}'), (shell, 'SpriteSizeMlt', f'{CASING_SCALE}'),
                             (vfx, 'LaunchSound', LAUNCH_SOUND),
                             (vfx, 'LaunchScale', f'{LAUNCH_SCALE},{LAUNCH_SCALE},{LAUNCH_SCALE}'),
                             (wreck, 'ActorToSpawn', f'{WRECK}.DestroyedTW_53K_C')):
        act('set_pin_default', blueprint_path=TEAM_WEAPON, graph_name='EventGraph', node_id=node, pin_name=pin, value=value)
    compiled = act('compile_blueprint', blueprint_path=TEAM_WEAPON)
    print('team weapon compile errors', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TEAM_WEAPON)
    run_python(f"""
import unreal
bp = unreal.load_asset('{TEAM_WEAPON}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
config = cdo.get_editor_property('M_TeamWeaponConfig')
embedded = config.get_editor_property('m_embedded_settings')
embedded.set_editor_property('turret_height', {BORE_HEIGHT})
config.set_editor_property('m_embedded_settings', embedded)
cdo.set_editor_property('M_TeamWeaponConfig', config)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
print('TW turret height', cdo.get_editor_property('M_TeamWeaponConfig').get_editor_property('m_embedded_settings').get_editor_property('turret_height'))
""", '53K team weapon config')


def build_squad():
    duplicate(ZIS + '/Rus_TWSquad_Zis3_76', SQUAD)
    run_python(f"""
import unreal
bp = unreal.load_asset('{SQUAD}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
cdo.set_editor_property('TeamWeaponClass', unreal.load_asset('{TEAM_WEAPON}').generated_class())
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print('SQUAD team weapon', unreal.get_default_object(bp.generated_class()).get_editor_property('TeamWeaponClass'))
""", '53K squad team weapon class')


def register_spawner():
    run_python(f"""
import unreal
bp = unreal.load_asset('{SPAWNER}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
rows = [r for r in cdo.get_editor_property('SetupRusSquads')
        if r.get_editor_property('squad_subtype') != unreal.SquadSubtype.SQUAD_RUS_53K_45MM]
row = unreal.SquadTrainingOptionSetup()
row.set_editor_property('squad_subtype', unreal.SquadSubtype.SQUAD_RUS_53K_45MM)
row.set_editor_property('unit_class', unreal.load_asset('{SQUAD}').generated_class())
rows.append(row)
cdo.set_editor_property('SetupRusSquads', rows)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
print('SPAWNER rus squads', len(cdo.get_editor_property('SetupRusSquads')))
""", '53K spawner row')


if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if which in ('all', 'wreck'):
        build_wreck()
    if which in ('all', 'weapon'):
        build_team_weapon()
    if which in ('all', 'squad'):
        build_squad()
    if which in ('all', 'spawner'):
        register_spawner()
