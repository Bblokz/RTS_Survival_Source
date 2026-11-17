"""Job 2b (runs outside the editor): point every sequence player of AnimBp_PanzerWerfer_Tracks at the Panzerwerfer
anims, compile, save. Run after pw_animbp.py: python pw_animbp_nodes.py"""
import json

import pw_uecp as u

PW = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Panzerwerfer'
ABP = PW + '/AnimBp_PanzerWerfer_Tracks'
SEQ_MAP = {
    'Anim_Sdkfz-250_Tracks_FWD': 'Anim_PanzerWerfer_Tracks_FWD',
    'Anim_Sdkfz-250_Tracks_BWD': 'Anim_PanzerWerfer_Tracks_BWD',
    'Anim_Sdkfz-250_Tracks_TPose': 'Anim_PanzerWerfer_Tracks_TPose',
    'Anim_Sdkfz-250_Tracks_TurnLeft': 'Anim_PanzerWerfer_Tracks_TurnLeft',
    'Anim_Sdkfz-250_Tracks_TurnRight': 'Anim_PanzerWerfer_Tracks_TurnRight',
    'Anim_Sdkfz-250_Tracks_StationaryLeft': 'Anim_PanzerWerfer_Tracks_StationaryLeft',
    'Anim_Sdkfz-250_Tracks_StationaryRight': 'Anim_PanzerWerfer_Tracks_StationaryRight',
}

nodes = u.act('get_anim_graph_nodes', anim_blueprint_path=ABP)['nodes']
done = 0
for n in nodes:
    if n['node_class'] != 'AnimGraphNode_SequencePlayer':
        continue
    old = n['node_title'].split('\n')[0]
    new = SEQ_MAP.get(old)
    if new is None:
        print('UNMAPPED', n['node_title'])
        continue
    r = u.act('set_anim_node_property', anim_blueprint_path=ABP, node_guid=n['node_guid'], property_name='Sequence',
              property_value=PW + '/Anims/' + new + '.' + new)
    print(old, '->', new, r.get('ok'), '' if r.get('ok') else json.dumps(r)[:300])
    done += 1
print('players', done)
print(json.dumps(u.act('compile_blueprint', blueprint_path=ABP))[:600])
after = [n['node_title'].split('\n')[0] for n in u.act('get_anim_graph_nodes', anim_blueprint_path=ABP)['nodes']
         if n['node_class'] == 'AnimGraphNode_SequencePlayer']
print('AFTER', after)
print(u.py("import unreal\nprint(unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset('%s'), False))" % ABP))
