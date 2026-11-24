"""Make the two Ore counterattack settings assets (copies of DA_Berlin_ResponseForce, road group only: no train, no
air drops, no dig-in intro, no cinematic) and point the two counterattack stages at them.

Light (after convoy 2): infantry and light armour. Heavy (after convoy 5): medium and heavy armour with hazmat
engineers. Wave sizes per difficulty are starter values for the owner to tune. Usage: python make_counterattack_settings.py
"""
import ue

SRC = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/DA_Berlin_ResponseForce'
UNITS = {
    'Light': {'tanks': ['BT7', 'T26', 'T70', 'BA12', 'BA14'],
              'squads': ['RUS_MOSIN', 'RUS_OKHOTNIK', 'RUS_MAXIM']},
    'Heavy': {'tanks': ['T34_76', 'T34_85', 'T34_85', 'KV_1', 'SU_85', 'IS_2'],
              'squads': ['RUS_HAZMAT_ENGINEERS', 'RUS_HAZMAT_BAZOOKA', 'RUS_LARGE_PTRS']},
}
CODE = r'''
import unreal
src = '%(src)s'
for kind, units in %(units)r.items():
    path = '%(base)sDA_Ore_%%sCounterattack' %% kind
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.duplicate_asset(src, path)
    d = unreal.load_asset(path)
    train = d.get_editor_property('train')
    train.set_editor_property('train_class', None)
    train.set_editor_property('path_points', [])
    d.set_editor_property('train', train)
    spawns = d.get_editor_property('spawns')
    spawns.set_editor_property('center_spawn_location', unreal.Vector(2010, 22500, 1400))
    spawns.set_editor_property('landing_pad_centers', [])
    d.set_editor_property('spawns', spawns)
    # Down the valley road from the convoy yard, past the ambush corridor, then attack-move up to the HQ.
    d.set_editor_property('attack_route', [unreal.Vector(1000, 15000, 0), unreal.Vector(-2800, 1842, 52)])
    d.set_editor_property('fallback_hq_location', unreal.Vector(-21000, 8000, 1800))
    for diff in ('new_to_rts', 'normal', 'hard', 'brutal', 'ironman'):
        s = d.get_editor_property(diff)
        s.set_editor_property('tank_types', [getattr(unreal.TankSubtype, 'TANK_' + t) for t in units['tanks']])
        s.set_editor_property('squad_types', [getattr(unreal.SquadSubtype, 'SQUAD_' + q) for q in units['squads']])
        if kind == 'Light':
            s.set_editor_property('wave_count', 2)
        d.set_editor_property(diff, s)
    print(kind, unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False))
    stage = unreal.load_class(None, '%(base)sMission_Ore_%%sCounterattack.Mission_Ore_%%sCounterattack_C' %% (kind, kind))
    unreal.get_default_object(stage).set_editor_property('settings', d)
'''


def main():
    print(ue.py(CODE % {'src': SRC, 'units': UNITS, 'base': ue.BASE}).get('output_text'))
    for kind in UNITS:
        path = ue.BASE + f'Mission_Ore_{kind}Counterattack'
        ue.a('compile_blueprint', blueprint_path=path)
        ue.py(f"import unreal\nprint(unreal.EditorAssetLibrary.save_asset('{path}', only_if_is_dirty=False))")


if __name__ == '__main__':
    main()
