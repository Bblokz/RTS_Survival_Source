"""Add dense instanced meadow vegetation and bury the backdrop skirt below the terrain."""
import math
import random
import unreal

subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith("/Game/RTS_Survival/Maps/Shorts/RTS_Shorts.")
actors=subsystem.get_all_level_actors()
for actor in actors:
    if actor.get_actor_label().startswith("PM_Polish_"):
        subsystem.destroy_actor(actor)

hills=[(-4000,-6500,3200,410),(2000,-7200,3500,560),(6500,-5200,2300,340),
       (-5000,6900,3300,480),(1000,6800,2900,430),(6800,6600,3600,690),
       (10500,0,3100,380),(-10500,2500,3400,320)]
def height(x,y):
    edge=max(0,min(1,(math.hypot(x,y)-6500)/7000))
    result=edge*(75+65*math.sin(x/4300)*math.cos(y/5100))
    for centre_x,centre_y,radius,peak in hills:
        distance=math.hypot(x-centre_x,y-centre_y)/radius
        if distance<1:
            result+=peak*(1-distance*distance)**2
    return result
def mark(actor,label):
    actor.set_actor_label(label)
    actor.set_folder_path("Shorts/ParatrooperMeadow/Polish")
    actor.set_editor_property("tags",[unreal.Name("ShortsOnly_ParatrooperMeadow")])
    return actor

# The distance meshes have wide, flat skirts; submerge those skirts instead of showing a hard black edge.
for actor in actors:
    if actor.get_actor_label().startswith("PM_Mountain_"):
        bounds=actor.static_mesh_component.static_mesh.get_bounds()
        bottom=(bounds.origin.z-bounds.box_extent.z)*actor.get_actor_scale3d().z
        point=actor.get_actor_location()
        point.z=80-bottom-700
        actor.set_actor_location(point,False,False)

blueprint=unreal.load_asset("/Game/RTS_Survival/Shorts/BP_ParatrooperMeadowGrass")
assert blueprint
unreal.EditorAssetLibrary.save_loaded_asset(blueprint,False)
container=mark(subsystem.spawn_actor_from_class(blueprint.generated_class(),unreal.Vector(230000,10000,144)),"PM_Polish_DenseMeadow")
components={component.get_name():component for component in container.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)}
settings={"MeadowGrass":"SM_GrassBig","WildGrass":"SM_WildGrass_001","MeadowFlowers":"SM_SmallFlowers"}
for name,mesh_name in settings.items():
    component=components[name]
    component.set_static_mesh(unreal.load_asset("/Game/UVB80/Assets/UVB80/Environment/Grass/"+mesh_name))
    component.set_collision_profile_name("NoCollision")
    component.set_cast_shadow(False)
    component.set_cull_distances(18000,23000)

rng=random.Random(9418)
instances={name:[] for name in settings}
def add_grass(name,x,y,wide,tall):
    instances[name].append(unreal.Transform(location=unreal.Vector(x,y,height(x,y)-1),rotation=unreal.Rotator(pitch=0,yaw=rng.uniform(0,360),roll=0),scale=unreal.Vector(wide,wide,tall)))

# Overlapping patches create natural masses of grass, rather than isolated evenly spaced tufts.
for side in [-1,1]:
    for patch in range(60):
        centre_x=rng.uniform(-8500,11500)
        centre_y=side*rng.uniform(3100,8500)
        radius=rng.uniform(300,700)
        for index in range(220):
            angle=rng.uniform(0,math.tau)
            distance=radius*math.sqrt(rng.random())
            x=centre_x+math.cos(angle)*distance
            y=centre_y+math.sin(angle)*distance
            if abs(y)<2800 and abs(x)<5500:
                continue
            name="MeadowFlowers" if index%17==0 else "WildGrass" if index%4==0 else "MeadowGrass"
            add_grass(name,x,y,rng.uniform(1.5,2.8),rng.uniform(.85,1.6))
# Keep the short grass low in the central clearing and leave each starting tank footprint and drop ring free.
for index in range(2200):
    x=rng.uniform(-5000,6000)
    y=rng.uniform(-2800,2800)
    if math.hypot(x,y)<850:
        continue
    if min(abs(x-2000),abs(x+2000))<500 and -1900<y<2600:
        continue
    add_grass("MeadowGrass",x,y,rng.uniform(.8,1.5),rng.uniform(.3,.55))
for name,transforms in instances.items():
    components[name].add_instances(transforms,False,False,False)

tree_root="/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/"
for side in [-1,1]:
    for index,x in enumerate([-6500,-4300,-700,1000,4300,6700,8700]):
        y=side*(3300+index%3*350)
        asset=tree_root+(["Firs/BP_WpoFir","Spruces/BP_WpoSpruce_Tall","Firs/BP_WpoFir_Small"][index%3])
        cls=unreal.load_class(None,asset+"."+asset.rsplit("/",1)[1]+"_C")
        assert cls,asset
        actor=mark(subsystem.spawn_actor_from_class(cls,unreal.Vector(230000+x,10000+y,144+height(x,y)),unreal.Rotator(pitch=0,yaw=rng.uniform(0,360),roll=0)),f"PM_Polish_HeroTree_{side}_{index}")
        scale=rng.uniform(1.05,1.45)
        actor.set_actor_scale3d(unreal.Vector(scale,scale,scale))

# A private material instance fills out the leaf canopies while retaining their original wind functions.
leaf_path="/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/LeafTrees/Mats/Tree_01_M_Inst"
leaf_parent=unreal.load_asset(leaf_path)
leaf_material=unreal.load_asset("/Game/RTS_Survival/Shorts/MI_ParatrooperMeadowLeaves")
if not leaf_material:
    leaf_material=unreal.AssetToolsHelpers.get_asset_tools().create_asset("MI_ParatrooperMeadowLeaves","/Game/RTS_Survival/Shorts",unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
unreal.MaterialEditingLibrary.set_material_instance_parent(leaf_material,leaf_parent)
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(leaf_material,"Opacity Intensity",1.5)
unreal.EditorAssetLibrary.save_loaded_asset(leaf_material,False)
for actor in actors:
    if actor.get_actor_label().startswith("PM_Dress_Bushes_"):
        scale=1.05+rng.random()*.65
        actor.set_actor_scale3d(unreal.Vector(scale,scale,scale))
    if actor.get_actor_label().startswith("PM_Dress_WindTrees_"):
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            for slot in range(component.get_num_materials()):
                material=component.get_material(slot)
                if material and material.get_path_name().startswith(leaf_path):
                    component.set_material(slot,leaf_material)

unreal.EditorAssetLibrary.save_loaded_asset(blueprint,False)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
print("POLISH_COMPLETE",{name:len(values) for name,values in instances.items()},"hero_wind_trees",14)
