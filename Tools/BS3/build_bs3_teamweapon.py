"""Builds the BS-3 team weapon, its squad, its wreck and the spawner entry through the editor bridge.

Copies of the ZiS-3's (same skeleton, same kind of weapon) with the BS-3's assets:
  DestroyedTW_BS3           the scavengeable wreck, SM_Destroyed_BS3
  Rus_TW_BS3_100mm          the gun: SK_TW_BS3 with AnimBp_TW_BS3, weapon BS3_100MM_TW fired from LaunchBS3 (casings
                            from OutBS3), the 100 mm LB-1 gun's sound, crew spots moved back to its breech, between the trails, turret
                            height at the BS-3's bore, spawns DestroyedTW_BS3 when destroyed
  Rus_TWSquad_BS3_100mm     the squad: the ZiS-3's Soviet crew with TeamWeaponClass = Rus_TW_BS3_100mm
  RTS_BP_RTSAsyncSpawner    SetupRusSquads row Squad_Rus_BS3_100MM -> Rus_TWSquad_BS3_100mm
The crew animations need no setup: the squad unit anim blueprint's crew montages fall back to their base entries
(the anti-tank gun loader and spotter) for a subtype without an override.

Run: python build_bs3_teamweapon.py
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/BS3_100MM'
ZIS = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/Zis_76MM'
WRECK = ROOT + '/DestroyedTW_BS3'
TEAM_WEAPON = ROOT + '/Rus_TW_BS3_100mm'
SQUAD = ROOT + '/Rus_TWSquad_BS3_100mm'
SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
LAUNCH_SOUND = '/Game/RTS_Survival/Sounds/WeaponSounds/HeavyCaliber/DT100mm/S_FireWeapon_LB_1_100mm.S_FireWeapon_LB_1_100mm'
# Crew spots (Unreal gun frame): the BS-3's breech is at x -87; the loader stands beside it, between the wide open
# trails (66 cm inside the left one), the spotter further back on the other side.
CREW_SPOTS = {'Loader': (-165.0, 72.0), 'Spotter': (-225.0, -82.0)}
CREW_SPOT_HEIGHT = 21.165
BORE_HEIGHT = 107.0
LAUNCH_SCALE = 1.3
CASING_SCALE = 1.25


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
""", 'BS3 duplicate ' + target.split('/')[-1])


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
    act('set_static_mesh_component', blueprint_path=WRECK, component_name='Weapon', mesh_path=ROOT + '/Mesh/SM_Destroyed_BS3')
    compiled = act('compile_blueprint', blueprint_path=WRECK)
    print('wreck compile errors', compiled.get('error_count'))
    act('save_asset', asset_path=WRECK)


def build_team_weapon():
    duplicate(ZIS + '/Rus_TW_Zis3_76', TEAM_WEAPON)
    # The bridge's mesh + anim BP check compares skeleton paths in two spellings and refuses this valid pair, so the
    # anim class is set as a plain component property.
    act('set_skeletal_mesh_component', blueprint_path=TEAM_WEAPON, component_name='WeaponMesh',
        mesh_path=ROOT + '/Mesh/SK_TW_BS3')
    act('edit_component_property', blueprint_path=TEAM_WEAPON, component_name='WeaponMesh', property_name='AnimClass',
        value=f"{ROOT}/AnimBp_TW_BS3.AnimBp_TW_BS3_C")
    for name, (x, y) in CREW_SPOTS.items():
        act('set_component_transform', blueprint_path=TEAM_WEAPON, component_name=name, location=[x, y, CREW_SPOT_HEIGHT])
    act('set_blueprint_variable_default', blueprint_path=TEAM_WEAPON, variable_name='Weapon Name', default_value='BS3_100MM_TW')
    nodes = nodes_of(TEAM_WEAPON)

    def struct(name):
        return lambda n: n.get('op') == 'make_struct' and n.get('detail', {}).get('struct') == name
    weapon = one_node(nodes, struct('InitWeaponStateProjectile'), 'weapon struct')
    shell = one_node(nodes, struct('WeaponShellCase'), 'shell case')
    vfx = one_node(nodes, struct('WeaponVFX'), 'weapon vfx')
    wreck = one_node(nodes, struct('DestroySpawnActor'), 'wreck spawn')
    for node, pin, value in ((weapon, 'FireSocketName', 'LaunchBS3'), (shell, 'ShellCaseSocketName', 'OutBS3'),
                             (shell, 'MeshSizeMlt', f'{CASING_SCALE}'), (shell, 'SpriteSizeMlt', f'{CASING_SCALE}'),
                             (vfx, 'LaunchSound', LAUNCH_SOUND),
                             (vfx, 'LaunchScale', f'{LAUNCH_SCALE},{LAUNCH_SCALE},{LAUNCH_SCALE}'),
                             (wreck, 'ActorToSpawn', f'{WRECK}.DestroyedTW_BS3_C')):
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
""", 'BS3 team weapon config')


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
""", 'BS3 squad team weapon class')


def register_spawner():
    run_python(f"""
import unreal
bp = unreal.load_asset('{SPAWNER}')
cdo = unreal.get_default_object(bp.generated_class())
bp.modify()
rows = [r for r in cdo.get_editor_property('SetupRusSquads')
        if r.get_editor_property('squad_subtype') != unreal.SquadSubtype.SQUAD_RUS_BS3_100MM]
row = unreal.SquadTrainingOptionSetup()
row.set_editor_property('squad_subtype', unreal.SquadSubtype.SQUAD_RUS_BS3_100MM)
row.set_editor_property('unit_class', unreal.load_asset('{SQUAD}').generated_class())
rows.append(row)
cdo.set_editor_property('SetupRusSquads', rows)
unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
print('SPAWNER rus squads', len(cdo.get_editor_property('SetupRusSquads')))
""", 'BS3 spawner row')


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
