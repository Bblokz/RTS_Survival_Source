"""Fresh-process check of the truck's hidden hit hull and the wreck's leftovers (DebugGame commandlet, -nullrhi)."""
import unreal
DEST = "/Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/"
L = []
def P(*a): L.append(" ".join(str(x) for x in a))
mesh = unreal.load_asset(DEST + "SM_OpelSupplyTruck_HitHull")
P("HITHULL flag", mesh.get_editor_property("body_setup").get_editor_property("collision_trace_flag"),
  "sockets", [(str(s.socket_name), s.relative_location.x, s.relative_location.y, s.relative_location.z) for s in mesh.get_sockets_by_tag("")])
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
truck = eas.spawn_actor_from_class(unreal.load_asset(DEST + "BP_Ger_OpelSupplyTruck").generated_class(), unreal.Vector(0, 0, 0))
hull = [c for c in truck.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "Hull"][0]
P("HULL collision", hull.get_collision_enabled(), hull.get_collision_object_type(), hull.get_collision_profile_name())
RAYS = {"cab_side": ((150, -600, 170), (150, 600, 170)), "bed_boards_side": ((-150, -600, 150), (-150, 600, 150)),
        "rear": ((-900, 0, 150), (900, 0, 150)), "front": ((900, 0, 110), (-900, 0, 110)), "top_bed": ((-150, 0, 900), (-150, 0, -100)),
        "above_truck_miss": ((-900, 0, 400), (900, 0, 400))}
for name, (a, b) in RAYS.items():
    for complex_trace in (False, True):
        r = hull.line_trace_component(unreal.Vector(*a), unreal.Vector(*b), complex_trace, False, False)
        hit = r[0] if isinstance(r, tuple) and r and isinstance(r[0], unreal.Vector) else r
        P("RAY", name, "complex" if complex_trace else "simple", hit)
# Control: the template's hull with the same rays (it is taller and wider, so every ray but the miss should hit).
zil = eas.spawn_actor_from_class(unreal.EditorAssetLibrary.load_blueprint_class(
    "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilCargo/Rus_ArmoredCar_ZilFuel"), unreal.Vector(0, 0, 0))
zil_hull = [c for c in zil.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "Hull"][0]
for name, (a, b) in RAYS.items():
    r = zil_hull.line_trace_component(unreal.Vector(*a), unreal.Vector(*b), True, False, False)
    P("ZIL_RAY", name, r[0] if r else None)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for name, (a, b) in RAYS.items():
    hits = unreal.SystemLibrary.line_trace_multi(world, unreal.Vector(*a), unreal.Vector(*b), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                 True, [], unreal.DrawDebugTrace.NONE, True) or []
    P("WORLD_RAY", name, [(h.to_tuple()[9].get_name() if h.to_tuple()[9] else None, h.to_tuple()[10].get_name() if h.to_tuple()[10] else None) for h in hits])
wreck = eas.spawn_actor_from_class(unreal.load_asset(DEST + "Env_Dest_OpelSupplyTruck").generated_class(), unreal.Vector(0, 4000, 0))
for c in wreck.get_components_by_class(unreal.GeometryCollectionComponent):
    rc = c.get_editor_property("rest_collection")
    P("WRECK geometry collection", c.get_name(), rc.get_path_name() if rc else None)
open("D:/UE5Projects/RTS_Survival/Saved/SupplyTruckBP/verify/hits.txt", "w").write("\n".join(L))
