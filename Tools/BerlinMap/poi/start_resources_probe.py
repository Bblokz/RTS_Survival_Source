# Probe (read-only): spawn each Radixite / Metal resource BP far away, print its components, mesh sizes and resource
# settings, then destroy it. Sent to the throwaway editor with berlin_cmd.py.
import unreal

ROOT = "/Game/RTS_Survival/Blueprints/Resources/ChildBps/"
NAMES = ["Radixite/Resource_Radixite_Crater0", "Radixite/Resource_Radixite_Crystal", "Radixite/Resource_Radixite_Crystal_Large",
         "Radixite/Resource_Radixite_Crystal_LargePoint", "Radixite/Resource_Radixite_Crystal_LargeWithGrowth",
         "Radixite/Resource_Radixite_Crystal_NoDecal", "Radixite/Resource_Radixite_Crystal_SmallDecal",
         "Radixite/Resource_Radixite_Crystal_SmDecal_Growth",
         "Metal/Resource_MetalTower", "Metal/Resource_Metal_Barrels", "Metal/Resource_Metal_BrokenWaterTank",
         "Metal/Resource_Metal_Rails", "Metal/Resource_Metal_Ruin1"]
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for old in sub.get_all_level_actors():
    if old.get_actor_location().z < -150000:
        print('removing leftover', old.get_name()); sub.destroy_actor(old)
for n in NAMES:
    cls = unreal.EditorAssetLibrary.load_blueprint_class(ROOT + n)
    if cls is None:
        print(n, "MISSING")
        continue
    a = sub.spawn_actor_from_class(cls, unreal.Vector(0, 0, -200000), unreal.Rotator(0, 0, 0))
    print("==", n, a.get_class().get_name(), "parent", unreal.SystemLibrary.get_class_display_name(cls))
    for c in a.get_components_by_class(unreal.ActorComponent):
        line = "   %s (%s)" % (c.get_name(), c.get_class().get_name())
        if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
            b = c.static_mesh.get_bounds()
            s = c.get_world_scale()
            line += " mesh %s ext %.0fx%.0fx%.0f scale %.2f rel %s" % (
                c.static_mesh.get_name(), b.box_extent.x * s.x, b.box_extent.y * s.y, b.box_extent.z * s.z, s.x,
                c.get_editor_property('relative_location'))
        if isinstance(c, unreal.DecalComponent):
            line += " decal size %s" % c.decal_size
        if "esource" in c.get_class().get_name():
            for p in ("resource_type", "total_amount", "amount_per_harvest"):
                try:
                    line += " %s=%s" % (p, c.get_editor_property(p))
                except Exception:
                    pass
            line += " props:" + ",".join(x for x in dir(c) if not x.startswith("_") and any(k in x.lower() for k in ("amount", "resource", "capacity", "harvest")))[:600]
        print(line)
    sub.destroy_actor(a)
