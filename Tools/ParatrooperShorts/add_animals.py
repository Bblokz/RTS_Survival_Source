"""Place ambient VAT herds only on the paratrooper meadow's two flanks."""
import json
import unreal

subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith("/Game/RTS_Survival/Maps/Shorts/RTS_Shorts.")
actors=subsystem.get_all_level_actors()
def transform_values(actor):
    point=actor.get_actor_location()
    rotation=actor.get_actor_rotation()
    scale=actor.get_actor_scale3d()
    return (point.x,point.y,point.z,rotation.pitch,rotation.yaw,rotation.roll,scale.x,scale.y,scale.z)
original={a.get_path_name():transform_values(a) for a in actors if not a.get_actor_label().startswith("PM_Animal_")}
for actor in actors:
    if actor.get_actor_label().startswith("PM_Animal_"):
        subsystem.destroy_actor(actor)

herd=[("Sheep",-1200,2580,25),("Cow",-550,2820,-20),("Sheep",60,2530,145),
      ("Goat",680,2760,205),("Sheep",1260,2540,75)]
created=[]
for side_name,side in [("Left",-1),("Right",1)]:
    for index,(animal,local_x,distance,yaw) in enumerate(herd):
        x=230000+local_x
        y=10000+side*distance
        hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,3000),unreal.Vector(x,y,-500),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE)
        assert hit,(animal,x,y)
        hit_data=hit.to_tuple()
        assert isinstance(hit_data[9],unreal.LandscapeProxy),(animal,hit_data[9])
        point=hit_data[5]
        projected=unreal.NavigationSystemV1.project_point_to_navigation(world,point,None,None,unreal.Vector(180,180,400))
        assert projected,(animal,point)
        assert abs(projected.x-x)<180 and abs(projected.y-y)<180,(animal,projected)
        path=f"/Game/RTS_Survival/Blueprints/Environment/Animated/FarmAnimals/{animal}/BP_Wanderer_{animal}"
        cls=unreal.load_class(None,path+f".BP_Wanderer_{animal}_C")
        assert cls,path
        actor=subsystem.spawn_actor_from_class(cls,point,unreal.Rotator(pitch=0,yaw=yaw if side<0 else -yaw,roll=0))
        actor.set_actor_label(f"PM_Animal_{side_name}_{animal}_{index:02d}")
        actor.set_folder_path("Shorts/ParatrooperMeadow/Animals/"+side_name)
        actor.set_editor_property("tags",[unreal.Name("ShortsOnly_ParatrooperMeadow")])
        movement=actor.get_editor_property("MovementSettings")
        movement.set_editor_property("WanderRadius",180)
        movement.set_editor_property("MinWalkDistance",70)
        actor.set_editor_property("MovementSettings",movement)
        vat=actor.get_editor_property("VatSettings")
        assert vat.get_editor_property("WalkClip").get_editor_property("NumFrames")>1
        assert vat.get_editor_property("IdleClip").get_editor_property("NumFrames")>1
        mesh=actor.get_editor_property("AnimalMesh")
        assert "_VAT" in mesh.static_mesh.get_name()
        created.append({"label":actor.get_actor_label(),"location":[point.x,point.y,point.z],"wander_radius_cm":180,"mesh":mesh.static_mesh.get_path_name()})

for actor in subsystem.get_all_level_actors():
    if actor.get_path_name() in original:
        assert transform_values(actor)==original[actor.get_path_name()],actor.get_actor_label()
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
print(json.dumps({"animals":created,"total":len(created),"other_actors_unchanged":len(original)},indent=2))
