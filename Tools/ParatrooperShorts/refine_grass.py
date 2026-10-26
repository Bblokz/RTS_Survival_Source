"""Keep meadow colours and wind materials private to this short's foliage container."""
import unreal
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor=next(a for a in subsystem.get_all_level_actors() if a.get_actor_label()=="PM_Polish_DenseMeadow")
components={c.get_name():c for c in actor.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)}
for name,asset_name,mesh_name in [("MeadowGrass","MI_ParatrooperMeadowGrass","SM_GrassBig"),("WildGrass","MI_ParatrooperMeadowTallGrass","SM_TallGrass")]:
    mesh=unreal.load_asset("/Game/UVB80/Assets/UVB80/Environment/Grass/"+mesh_name)
    component=components[name]
    component.set_static_mesh(mesh)
    material=unreal.load_asset("/Game/RTS_Survival/Shorts/"+asset_name)
    if not material:
        material=unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name,"/Game/RTS_Survival/Shorts",unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(material,mesh.get_material(0))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(material,"Tint",unreal.LinearColor(.78,1.1,.76,1))
    component.set_material(0,material)
    unreal.EditorAssetLibrary.save_loaded_asset(material,False)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
print("Saved fresh meadow colours and tall wind grass on the shoulders.")
