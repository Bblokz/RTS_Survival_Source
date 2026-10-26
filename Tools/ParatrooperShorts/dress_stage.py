"""Idempotent isolated meadow dressing; all actors use the stage visibility tag."""
import math
import random
import struct
from pathlib import Path
import unreal

subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith("/Game/RTS_Survival/Maps/Shorts/RTS_Shorts.")
actors=subsystem.get_all_level_actors()
landscape=next(a for a in actors if a.get_actor_label()=="PM_Landscape")
def transform_values(actor):
    point=actor.get_actor_location()
    rotation=actor.get_actor_rotation()
    scale=actor.get_actor_scale3d()
    return (point.x,point.y,point.z,rotation.pitch,rotation.yaw,rotation.roll,scale.x,scale.y,scale.z)
original={a.get_path_name():transform_values(a) for a in actors if not a.actor_has_tag("ShortsOnly_ParatrooperMeadow")}
for actor in actors:
    if actor.get_actor_label().startswith("PM_Dress_"):
        subsystem.destroy_actor(actor)

# The middle stays exactly as it was. Smooth low ridges sit outside all tank lanes.
hills=[(-4000,-6500,3200,410),(2000,-7200,3500,560),(6500,-5200,2300,340),
       (-5000,6900,3300,480),(1000,6800,2900,430),(6800,6600,3600,690),
       (10500,0,3100,380),(-10500,2500,3400,320)]
def ground_height(local_x,local_y):
    edge_blend=max(0,min(1,(math.hypot(local_x,local_y)-6500)/7000))
    height=edge_blend*(75+65*math.sin(local_x/4300)*math.cos(local_y/5100))
    for hill_x,hill_y,radius,peak in hills:
        distance=math.hypot(local_x-hill_x,local_y-hill_y)/radius
        if distance<1:
            height+=peak*(1-distance*distance)**2
    return height
heights=[32768+round(ground_height((column-189)*100,(row-189)*100)*128/100) for row in range(379) for column in range(379)]
work=Path("D:/UE5Projects/RTS_Survival/Source/Tools/ParatrooperShorts")
height_file=work/"dressed_height.raw"
height_file.write_bytes(struct.pack("<%dH"%len(heights),*heights))
assert unreal.RTSShortsLandscapeLibrary.import_stage_heights(landscape,str(height_file))

counts={}
def mark(actor,kind):
    index=counts.get(kind,0)
    counts[kind]=index+1
    actor.set_actor_label(f"PM_Dress_{kind}_{index:03d}")
    actor.set_folder_path("Shorts/ParatrooperMeadow/"+kind)
    actor.set_editor_property("tags",[unreal.Name("ShortsOnly_ParatrooperMeadow")])
    return actor
def location(local_x,local_y,offset=0):
    return unreal.Vector(230000+local_x,10000+local_y,144+ground_height(local_x,local_y)+offset)
def spawn_blueprint(path,kind,local_x,local_y,scale=1,yaw=0):
    cls=unreal.load_class(None,path+"."+path.rsplit("/",1)[1]+"_C")
    assert cls,path
    actor=mark(subsystem.spawn_actor_from_class(cls,location(local_x,local_y),unreal.Rotator(pitch=0,yaw=yaw,roll=0)),kind)
    actor.set_actor_scale3d(unreal.Vector(scale,scale,scale))
    return actor

rng=random.Random(5621)
tree_root="/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/"
for side in [-1,1]:
    for index in range(29):
        local_x=rng.uniform(-9000,12000)
        local_y=side*rng.uniform(4100,10000)
        path=tree_root+(["LeafTrees/BP_WpoLeafTree_Large","SmallTrees/BP_WpoLeafTreeSmall","Spruces/BP_WpoSpruce_1"][index%3])
        spawn_blueprint(path,"WindTrees",local_x,local_y,rng.uniform(.4,.72),rng.uniform(0,360))
    for index in range(46):
        spawn_blueprint(tree_root+"LeafTrees/BP_WpoBush_"+str(1+index%2),"Bushes",rng.uniform(-7000,10000),side*rng.uniform(3000,7800),rng.uniform(.35,.7),rng.uniform(0,360))

grass_root="/Game/UVB80/Assets/UVB80/Environment/Grass/"
grass_meshes=[unreal.load_asset(grass_root+name) for name in ["SM_GrassBig","SM_GrassSmall","SM_WildGrass_001","SM_SmallFlowers"]]
assert all(grass_meshes)
for index in range(1600):
    local_x=rng.uniform(-8500,11500)
    local_y=rng.uniform(-7500,7500)
    is_clearing=abs(local_x)<4900 and abs(local_y)<2700
    if is_clearing and rng.random()<.72:
        continue
    actor=mark(subsystem.spawn_actor_from_class(unreal.StaticMeshActor,location(local_x,local_y,-2),unreal.Rotator(pitch=0,yaw=rng.uniform(0,360),roll=0)),"GrassFlowers")
    component=actor.static_mesh_component
    component.set_static_mesh(grass_meshes[index%len(grass_meshes)])
    component.set_collision_profile_name("NoCollision")
    scale=rng.uniform(.6,1.2)
    actor.set_actor_scale3d(unreal.Vector(scale,scale,scale*(.45 if is_clearing else .85)))

cover_root="/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/"
for index,(local_x,local_y,yaw) in enumerate([(-1500,-3100,15),(800,3200,-25),(3000,-3400,35),(-4400,2500,80),(4700,2700,-65),(-3500,-4200,130),(5000,-4700,45),(-600,4800,-70)]):
    spawn_blueprint(cover_root+f"Logs/BP_Cover_DeadTreeLog_{1+index%6:02d}","CoverLogs",local_x,local_y,1,yaw)
for index,(local_x,local_y,yaw) in enumerate([(-2600,-2700,0),(-2600,2900,0),(-3600,-500,0),(-3600,1200,0)]):
    path=cover_root+"SandBagWalls/"+["BP_Cover_RusSandbags_DoubleStraight","BP_Cover_RusSandbags_LongArc"][index%2]
    spawn_blueprint(path,"SovietSandbags",local_x,local_y,1,yaw)
for local_x,local_y,yaw in [(2600,-2700,180),(2600,2900,180),(3600,-700,180),(3600,1400,180)]:
    actor=spawn_blueprint("/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Ger_SandBagWall_FieldConstruction","GermanSandbags",local_x,local_y,1,yaw)
    rts=actor.get_component_by_class(unreal.RTSComponent)
    if rts:
        rts.set_editor_property("OwningPlayer",1)

# Dirt decals give the battle scar variety while preserving the driveable ground.
for index in range(32):
    local_x=rng.uniform(-5600,6600)
    local_y=rng.uniform(-4400,4400)
    radius=rng.uniform(180,430)
    actor=mark(subsystem.spawn_actor_from_class(unreal.DecalActor,location(local_x,local_y,60),unreal.Rotator(pitch=-90,yaw=rng.uniform(0,360),roll=0)),"Craters")
    decal=actor.get_component_by_class(unreal.DecalComponent)
    material=unreal.load_asset(f"/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_{1+index%6:02d}")
    assert material
    decal.set_decal_material(material)
    decal.set_editor_property("decal_size",unreal.Vector(180,radius,radius))
    decal.set_editor_property("fade_screen_size",.001)

# Existing grass and background trees follow the new shoulder heights too.
for actor in actors:
    if actor.get_actor_label().startswith(("PM_Grass_","PM_Tree_")):
        point=actor.get_actor_location()
        actor.set_actor_location(location(point.x-230000,point.y-10000,-2),False,False)
for actor in subsystem.get_all_level_actors():
    if actor.get_path_name() in original:
        assert transform_values(actor)==original[actor.get_path_name()],actor.get_actor_label()
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(225000,4000,6200),unreal.Rotator(pitch=-35,yaw=45,roll=0))
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
print("DRESSING_COMPLETE",counts,"centre_ground_cm",ground_height(0,0),"other_actors_unchanged",len(original))
