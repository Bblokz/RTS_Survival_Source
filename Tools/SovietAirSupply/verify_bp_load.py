# Loads BP_SovietAirSupplyPad and prints its parent class and key defaults (run on the Development editor DLL).
import unreal

OUT = unreal.Paths.project_saved_dir() + 'SovietAirSupply/verify_bp.txt'
bp = unreal.load_asset('/Game/RTS_Survival/Blueprints/Environment/SovietAirSupply/BP_SovietAirSupplyPad')
cls = bp.generated_class()
cdo = unreal.get_default_object(cls)
v = cdo.get_editor_property('worker_visuals')
s = cdo.get_editor_property('settings')
lines = [
    'is_pad=%s' % isinstance(cdo, unreal.SovietAirSupplyPad),
    'aircraft=%s' % s.get_editor_property('aircraft_class'),
    'points=%d/%d planes_every=%.0f' % (s.get_editor_property('training_points_per_delivery'),
                                        s.get_editor_property('building_points_per_delivery'),
                                        s.get_editor_property('seconds_between_planes')),
    'walk=%s carry=%s pick=%s put=%s' % (v.get_editor_property('walk_anim'), v.get_editor_property('carry_anim'),
                                         v.get_editor_property('pick_up_anim'), v.get_editor_property('put_down_anim')),
    'mast=%s' % cdo.get_editor_property('radio_mast_mesh').static_mesh,
]
with open(OUT, 'w') as fh:
    fh.write('\n'.join(lines))
