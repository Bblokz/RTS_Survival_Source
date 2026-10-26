"""Read-only checks for the saved meadow set and the Soviet drop Blueprint defaults."""
import json
from pathlib import Path
import unreal

subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=subsystem.get_all_level_actors()
by_label={a.get_actor_label():a for a in actors}
counts={}
for actor in actors:
    label=actor.get_actor_label()
    if label.startswith("PM_Dress_"):
        assert actor.actor_has_tag("ShortsOnly_ParatrooperMeadow"),label
        key=label.rsplit("_",1)[0]
        counts[key]=counts.get(key,0)+1
assert counts["PM_Dress_WindTrees"]==58
assert counts["PM_Dress_CoverLogs"]==8
defences=[a for a in actors if a.get_actor_label().startswith("PM_Defence_")]
assert sum(a.get_actor_label().startswith("PM_Defence_Soviet_") for a in defences)==13
assert sum(a.get_actor_label().startswith("PM_Defence_German_") for a in defences)==13
assert all(a.actor_has_tag("ShortsOnly_ParatrooperMeadow") for a in defences)
assert all(abs(a.get_actor_location().z-144)<1 for a in defences)
assert by_label["PM_Landscape"].get_editor_property("landscape_material").get_name()=="M_ParatrooperMeadowDiverseGround"
rvt=unreal.load_asset('/Game/RTS_Survival/Meshes/RVTs/RVT_Environment')
landscape=by_label['PM_Landscape']
assert rvt in landscape.get_editor_property('runtime_virtual_textures')
landscape_components=landscape.get_components_by_class(unreal.LandscapeComponent)
assert len(landscape_components)==36
assert all(rvt in component.get_editor_property('runtime_virtual_textures') for component in landscape_components)
volume=by_label['PM_RVT_Environment']
assert volume.actor_has_tag('ShortsOnly_ParatrooperMeadow')
assert volume.get_component_by_class(unreal.RuntimeVirtualTextureComponent).get_editor_property('virtual_texture')==rvt
assert abs(by_label['PM_Defence_Soviet_00'].get_actor_location().x-229200)<1
assert abs(by_label['PM_Defence_German_00'].get_actor_location().x-230550)<1
animals=[a for a in actors if a.get_actor_label().startswith("PM_Animal_")]
assert len(animals)==10
assert counts["PM_Dress_Craters"]==32
assert counts["PM_Dress_GrassFlowers"]>1000
polish=[a for a in actors if a.get_actor_label().startswith("PM_Polish_")]
assert len(polish)==15
grass=next(a for a in polish if a.get_actor_label()=="PM_Polish_DenseMeadow")
instances={c.get_name():c.get_instance_count() for c in grass.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)}
assert sum(instances.values())>25000
for name in ["Stage_Meadow","Stage_ParatrooperMeadow","PM_Landscape"]:
    assert name in by_label,name

cls=unreal.load_class(None,"/Game/RTS_Survival/Shorts/BP_ParatrooperMeadowDoubleRange.BP_ParatrooperMeadowDoubleRange_C")
default=unreal.get_default_object(cls)
multipliers=default.get_editor_property("BehaviourWeaponMultipliers")
assert multipliers.get_editor_property("RangeMlt")==2
assert default.get_editor_property("BehaviourLifeTime")==unreal.BehaviourLifeTime.NONE
assert default.get_editor_property("BehaviourStackRule")==unreal.BehaviourStackRule.NONE

owners={}
for path in ["/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin",
             "/Game/RTS_Survival/Blueprints/Squads/Rus/AssaultSquads/Rus_Squad_FlameROKS",
             "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Rus/RusBasic/Rus_Inf_Mosin",
             "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Rus/AssaultSquads/Rus_Inf_FlameROKS3",
             "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Rus/AssaultSquads/Rus_Inf_FlameROKS_Support"]:
    cls=unreal.load_class(None,path+"."+path.rsplit("/",1)[1]+"_C")
    assert cls,path
    actor=subsystem.spawn_actor_from_class(cls,unreal.Vector(220000,-5000,500))
    owners[path.rsplit("/",1)[1]]=actor.get_component_by_class(unreal.RTSComponent).get_editor_property("OwningPlayer")
    subsystem.destroy_actor(actor)

navigation={}
ground={}
for local_x,local_y in [(-2000,0),(0,0),(2000,0),(-2000,-1400),(2000,1400),(0,6800)]:
    x=230000+local_x
    y=10000+local_y
    hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,4000),unreal.Vector(x,y,-500),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE)
    assert hit,(x,y)
    values=hit.to_tuple()
    ground[f"{local_x},{local_y}"]=values[5].z
    if abs(local_y)<2000:
        point=unreal.NavigationSystemV1.project_point_to_navigation(world,unreal.Vector(x,y,180),None,None,unreal.Vector(200,200,500))
        navigation[f"{local_x},{local_y}"]=str(point)
        assert point,(x,y)
report={"dressing":counts,"defensive_sandbag_sections":len(defences),"vat_animals":len(animals),"grass_instances":instances,"hero_trees":14,"blueprint_owners":owners,"ground_cm":ground,"navigation":navigation,"range_multiplier":2}
Path("D:/UE5Projects/RTS_Survival/Source/Tools/ParatrooperShorts/audit_stage.json").write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
