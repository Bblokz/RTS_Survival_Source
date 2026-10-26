import unreal
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for path in ["/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin", "/Game/RTS_Survival/Blueprints/Squads/Rus/AssaultSquads/Rus_Squad_FlameROKS"]:
    cls=unreal.load_class(None,path+"."+path.rsplit("/",1)[1]+"_C")
    default=unreal.get_default_object(cls)
    print("SQUAD",path,"units",default.get_editor_property("SquadUnitClasses"))
    for soldier_class in set(default.get_editor_property("SquadUnitClasses")):
        # Soft class paths are resolved explicitly, without changing shared defaults.
        resolved=unreal.load_class(None,str(soldier_class).strip('"'))
        if not resolved:
            continue
        actor=subsystem.spawn_actor_from_class(resolved,unreal.Vector(220000,-5000,500))
        print("SOLDIER",resolved.get_path_name(),"owner",actor.get_component_by_class(unreal.RTSComponent).get_editor_property("OwningPlayer"))
        subsystem.destroy_actor(actor)
for path in ["/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/LeafTrees/BP_WpoLeafTree_Large", "/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/SmallTrees/BP_WpoLeafTreeSmall", "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/Logs/BP_Cover_DeadTreeLog_01", "/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Ger_SandBagWall_FieldConstruction"]:
    cls=unreal.load_class(None,path+"."+path.rsplit("/",1)[1]+"_C")
    actor=subsystem.spawn_actor_from_class(cls,unreal.Vector(220000,-5000,144))
    print("PROP",path, "parent",cls.get_super_class() if hasattr(cls,"get_super_class") else cls, "bounds",actor.get_actor_bounds(False))
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        print("MESH",component.get_name(),component.static_mesh, [str(component.get_material(i)) for i in range(component.get_num_materials())])
    subsystem.destroy_actor(actor)

name="BP_ParatrooperMeadowDoubleRange"
path="/Game/RTS_Survival/Shorts/"+name
blueprint=unreal.load_asset(path)
if not blueprint:
    factory=unreal.BlueprintFactory()
    factory.set_editor_property("parent_class",unreal.BehaviourWeapon)
    blueprint=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,"/Game/RTS_Survival/Shorts",unreal.Blueprint,factory)
assert blueprint
default=unreal.get_default_object(blueprint.generated_class())
multipliers=unreal.BehaviourWeaponMultipliers()
multipliers.set_editor_property("RangeMlt",2.0)
default.set_editor_property("BehaviourWeaponMultipliers",multipliers)
default.set_editor_property("BehaviourStackRule",unreal.BehaviourStackRule.NONE)
default.set_editor_property("BehaviourLifeTime",unreal.BehaviourLifeTime.NONE)
default.set_editor_property("M_TitleText","Meadow short: double range")
assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint,False)
print("RANGE_BEHAVIOUR",default.get_editor_property("BehaviourWeaponMultipliers"))
