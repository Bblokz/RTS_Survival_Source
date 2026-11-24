"""Fresh-load readback of the built Blueprints (spawned in the commandlet's editor world, nothing saved) plus the
footprints of the Base A classes for the layout check. Report: Saved/OreMountains/sites/ue_oreplant/verify.json"""
import json

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/ue_oreplant/verify.json'
ACT = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
S = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
D = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/'
R = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
CHECK = {
    'crusher': S + 'Sites/BaseA/BP_OreBaseA_CrusherHouse',
    'silos': S + 'Sites/BaseA/BP_OreBaseA_SiloGroup',
    'shed': S + 'Sites/BaseA/BP_OreBaseA_NissenShed',
    'heap': S + 'HeapStations/BP_OreHeapStation_Erzrolle',
    'parent_ammo': R + 'RusBuildings/Rus_Building_AmmoStorage',
    'parent_heap': S + 'HeapStations/BP_OreHeapStation',
}
FOOT = {
    'Yard_Hangar': D + 'Industrial/BP_Industrial_Dest_Hangar', 'Yard_Chimney': D + 'PostApo/BP_Dest_PostApo_Chimney',
    'Yard_WaterTower': D + 'PostApo/BP_Dest_PostApo_WaterTower', 'Yard_Fuel': R + 'RusBuildings/FuelStorage/BP_Dest_RusFuel1',
    'Yard_Stack': D + 'Airport/BP_Airport_Dest_CrateStack01', 'Yard_Barrels': D + 'Airport/BP_Airport_Dest_BarrelStack3',
    'Yard_Container': D + 'Industrial/BP_Industrial_Dest_Container',
    'Yard_Fence': D + 'Military/ConcreteWall/BP_Dest_PostApo_ConcreteFence',
    'tower_mg': R + 'RusBunkers/Rus_BXP_GuardTowerMG_Soviet', 'mg_bunker': R + 'RusBunkers/Rus_BXP_MGBunker',
    'ammo_store': R + 'RusBuildings/Rus_Building_AmmoStorage', 'fuel_store': R + 'RusBuildings/Rus_Building_FuelStorage',
    'supplies': R + 'RusBuildings/Rus_Building_Supplies',
    'crusher': CHECK['crusher'], 'silos': CHECK['silos'], 'shed': CHECK['shed'], 'heap': CHECK['heap'],
}
rep = {'bps': {}, 'foot': {}}


def comp_info(c):
    i = dict(cls=c.get_class().get_name())
    if isinstance(c, unreal.SceneComponent):
        l = c.get_editor_property('relative_location')
        s = c.get_editor_property('relative_scale3d')
        i['loc'] = [round(l.x, 1), round(l.y, 1), round(l.z, 1)]
        i['scale'] = [round(s.x, 3), round(s.y, 3), round(s.z, 3)]
        i['tags'] = [str(t) for t in c.get_editor_property('component_tags')]
        i['visible'] = c.get_editor_property('visible')
    if isinstance(c, unreal.StaticMeshComponent):
        m = c.get_editor_property('static_mesh')
        i['mesh'] = m.get_name() if m else None
        i['collision'] = str(c.get_collision_profile_name())
        i['nav'] = c.get_editor_property('can_ever_affect_navigation')
    if isinstance(c, unreal.BoxComponent):
        e = c.get_editor_property('box_extent')
        i['extent'] = [e.x, e.y, e.z]
    return i


for key, path in CHECK.items():
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    a = ACT.spawn_actor_from_class(cls, unreal.Vector(0, 0, 100000 + len(rep['bps']) * 10000))
    rep['bps'][key] = {c.get_name(): comp_info(c) for c in a.get_components_by_class(unreal.ActorComponent)}
    if key == 'heap' or key == 'parent_heap':
        try:
            cs = a.get_editor_property('capture_settings')
            ss = a.get_editor_property('station_settings')
            rep['bps'][key]['_settings'] = dict(units=cs.get_editor_property('amount_capture_units_needed'),
                                                seconds=cs.get_editor_property('capture_duration'),
                                                storage=ss.get_editor_property('storage_capacity'),
                                                health=ss.get_editor_property('held_health'))
        except Exception as e:  # noqa: BLE001
            rep['bps'][key]['_settings'] = repr(e)
    ACT.destroy_actor(a)

for key, path in FOOT.items():
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if cls is None:
        rep['foot'][key] = None
        continue
    a = ACT.spawn_actor_from_class(cls, unreal.Vector(0, 0, 200000), unreal.Rotator(0, 0, 0))
    o, e = a.get_actor_bounds(True)
    o2, e2 = a.get_actor_bounds(False)
    rep['foot'][key] = dict(colliding=dict(c=[round(o.x), round(o.y), round(o.z - 200000)], e=[round(e.x), round(e.y), round(e.z)]),
                            all=dict(c=[round(o2.x), round(o2.y), round(o2.z - 200000)], e=[round(e2.x), round(e2.y), round(e2.z)]))
    ACT.destroy_actor(a)

with open(OUT, 'w') as f:
    json.dump(rep, f, indent=1)
unreal.log_warning('OREPLANT_VERIFY done')
