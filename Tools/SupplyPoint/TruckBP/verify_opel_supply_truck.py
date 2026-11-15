"""Fresh-process check of BP_Ger_OpelSupplyTruck: T3D export + spawned component dump. DebugGame commandlet, -nullrhi."""
import unreal, os
OUT = "D:/UE5Projects/RTS_Survival/Saved/SupplyTruckBP/verify"
os.makedirs(OUT, exist_ok=True)
DEST = "/Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/"
L = []
def P(*a): L.append(" ".join(str(x) for x in a))
def t3d(obj, name):
    task = unreal.AssetExportTask()
    task.object = obj; task.filename = f"{OUT}/{name}.t3d"; task.exporter = unreal.ObjectExporterT3D()
    task.automated = True; task.prompt = False; task.replace_identical = True
    P("EXPORT", name, unreal.Exporter.run_asset_export_task(task))
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for i, n in enumerate(("BP_Ger_OpelSupplyTruck", "Env_Dest_OpelSupplyTruck")):
    bp = unreal.load_asset(DEST + n)
    t3d(bp, n)
    cls = bp.generated_class()
    cdo = unreal.get_default_object(cls)
    t3d(cdo, n + "_CDO")
    if i == 0:
        P("SUBTYPE", cdo.get_editor_property("ArmoredCarType"))
    a = eas.spawn_actor_from_class(cls, unreal.Vector(0, 4000 * i, 0))
    P("ACTOR", n, "root", a.root_component.get_name())
    for c in a.get_components_by_class(unreal.ActorComponent):
        s = f"  COMP {c.get_name()} {c.get_class().get_name()}"
        if isinstance(c, unreal.SceneComponent):
            par = c.get_attach_parent()
            wl = c.get_world_location() - a.get_actor_location()
            s += f" parent={par.get_name() if par else None} sock={c.get_attach_socket_name()} local=({wl.x:.1f},{wl.y:.1f},{wl.z:.1f}) rot={c.get_world_rotation()} scale={c.relative_scale3d} vis={c.is_visible()} hig={c.hidden_in_game}"
        if isinstance(c, unreal.StaticMeshComponent):
            s += f" mesh={c.static_mesh.get_name() if c.static_mesh else None} mats={[m.get_name() if m else None for m in c.get_materials()]}"
        if isinstance(c, unreal.SkeletalMeshComponent):
            sm = c.get_skeletal_mesh_asset()
            s += f" mesh={sm.get_name() if sm else None} anim={c.anim_class.get_name() if c.anim_class else None} mats={[m.get_name() if m else None for m in c.get_materials()]} anim_rate_scale={c.global_anim_rate_scale}"
        if isinstance(c, unreal.BoxComponent):
            s += f" scaledbox={c.get_scaled_box_extent()}"
        if c.get_class().get_name() == "RTSComponent":
            s += f" owning_player={c.get_editor_property('owning_player')}"
        P(s)
    o, e = a.get_actor_bounds(False)
    P("BOUNDS", o - a.get_actor_location(), e)
open(f"{OUT}/report.txt", "w").write("\n".join(L))
print("VERIFY_DONE")
