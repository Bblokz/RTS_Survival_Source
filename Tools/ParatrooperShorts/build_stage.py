"""Run through UECP execute_python on RTS_Shorts. Adds only the ParatrooperMeadow set."""
import json
import math
import random
import struct
from pathlib import Path
import unreal

subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith("/Game/RTS_Survival/Maps/Shorts/RTS_Shorts."), world
actors = subsystem.get_all_level_actors()
assert not any(a.get_actor_label().startswith("PM_Mountain_") for a in actors), "Completed scenery already exists; inspect before rebuilding."
def transform_values(actor):
    location=actor.get_actor_location()
    rotation=actor.get_actor_rotation()
    scale=actor.get_actor_scale3d()
    return (location.x,location.y,location.z,rotation.pitch,rotation.yaw,rotation.roll,scale.x,scale.y,scale.z)

original = {a.get_path_name():transform_values(a) for a in actors if not a.get_actor_label().startswith(("PM_","Stage_ParatrooperMeadow"))}
tag = unreal.Name("ShortsOnly_ParatrooperMeadow")
folder = "Shorts/ParatrooperMeadow"
centre = unreal.Vector(230000,10000,144)
work = Path("D:/UE5Projects/RTS_Survival/Source/Tools/ParatrooperShorts")
created = []


def mark(actor, label):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags",[tag])
    created.append(actor)
    return actor


def mesh_actor(mesh, label, location, scale, yaw=0, material=None):
    actor = mark(subsystem.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(pitch=0,yaw=yaw,roll=0)),label)
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_collision_profile_name("NoCollision")
    if material:
        for index in range(max(1,component.get_num_materials())):
            component.set_material(index,material)
    actor.set_actor_scale3d(scale)
    return actor


# A real landscape gives the aircraft ability a landscape trace and the squads a baked navigation surface.
sections = 6
quads = 63
size = sections*quads+1
height_file = work / "meadow_height.raw"
heights = []
for row in range(size):
    for column in range(size):
        local_x=(column-(size-1)/2)*100
        local_y=(row-(size-1)/2)*100
        edge_blend=max(0,min(1,(math.hypot(local_x,local_y)-6500)/7000))
        height_cm=edge_blend*(75+65*math.sin(local_x/4300)*math.cos(local_y/5100))
        heights.append(32768+round(height_cm*128/100))
height_file.write_bytes(struct.pack("<%dH"%len(heights),*heights))
material=unreal.load_asset("/Game/RTS_Survival/Shorts/M_ParatrooperMeadowGround")
assert material, "The dedicated world-mapped grass material must exist before building the stage."
landscape=next((a for a in actors if a.get_actor_label()=="PM_Landscape"),None)
landscape=landscape or unreal.RTSShortsLandscapeLibrary.create_painted_landscape(
    world,unreal.Vector(211100,-8900,144),unreal.Vector(100,100,100),sections,sections,quads,
    material,str(height_file),[],[],"PM_Landscape")
assert landscape, "Landscape import failed"
mark(landscape,"PM_Landscape")

source_stage=next(a for a in actors if a.get_actor_label()=="Stage_Meadow")
stage=next((a for a in actors if a.get_actor_label()=="Stage_ParatrooperMeadow"),None)
stage=mark(stage or subsystem.duplicate_actor(source_stage,world,unreal.Vector(242000,1000,0)),"Stage_ParatrooperMeadow")
stage.rename("Stage_ParatrooperMeadow")
stage.set_actor_location(unreal.Vector(228000,10000,144),False,False)

sun_source=next(a for a in actors if a.get_actor_label()=="Meadow_Sun")
sun=mark(subsystem.duplicate_actor(sun_source,world,unreal.Vector(0,0,0)),"ParatrooperMeadow_Sun")
sun.rename("ParatrooperMeadow_Sun")

for source_label,label in [("Meadow_Sky","PM_Sky"),("Meadow_Fog","PM_Fog")]:
    source=next(a for a in actors if a.get_actor_label()==source_label)
    mark(subsystem.duplicate_actor(source,world,unreal.Vector(0,0,0)),label)

nav_source=next(a for a in actors if a.get_actor_label()=="RTS_NavMeshVolume")
nav=mark(subsystem.duplicate_actor(nav_source,world,centre-nav_source.get_actor_location()),"PM_NavMeshBounds")
nav.set_actor_scale3d(unreal.Vector(185,185,45))

for index,(offset,yaw,width,depth,height) in enumerate([
    ((23000,-9000),-15,13000,18000,8500),((26000,14000),40,14000,18000,10500),
    ((3000,26000),105,18000,13000,7500),((-20000,22000),155,14000,16000,6500),
    ((23000,-30000),-45,16000,18000,6800),
]):
    number=index%4+1
    base=f"/Game/Brushify/DistanceMeshes/Desert_Mountain_{number:02d}/"
    mesh=unreal.load_asset(base+f"S_Desert_Mountain_{number:02d}")
    grasslands=unreal.load_asset(base+f"MI_Desert_Mountain_{number:02d}_Grasslands")
    assert mesh and grasslands
    bounds=mesh.get_bounds()
    scale=unreal.Vector(width/max(1,bounds.box_extent.x),depth/max(1,bounds.box_extent.y),height/max(1,2*bounds.box_extent.z))
    bottom=(bounds.origin.z-bounds.box_extent.z)*scale.z
    mesh_actor(mesh,f"PM_Mountain_{index:02d}",unreal.Vector(centre.x+offset[0],centre.y+offset[1],80-bottom),scale,yaw,grasslands)

rng=random.Random(20261025)
grass_meshes=[unreal.load_asset(p) for p in [
    "/Game/UVB80/Assets/UVB80/Environment/Grass/SM_GrassBig",
    "/Game/UVB80/Assets/UVB80/Environment/Grass/SM_TallGrass",
    "/Game/UVB80/Assets/UVB80/Environment/Grass/SM_WildGrass_001"]]
for index in range(480):
    local_x=rng.uniform(-10000,12000)
    local_y=rng.uniform(-9000,9000)
    if abs(local_x)<3200 and abs(local_y)<2600:
        continue
    scale=rng.uniform(.8,1.5)
    mesh_actor(grass_meshes[index%len(grass_meshes)],f"PM_Grass_{index:03d}",
               unreal.Vector(centre.x+local_x,centre.y+local_y,150),unreal.Vector(scale,scale,scale),rng.uniform(0,360))

trees=[unreal.load_asset("/Game/UVB80/Assets/UVB80/Environment/Trees/"+name) for name in ["SM_PineSmall","SM_PineBig_001"]]
for index in range(36):
    angle=rng.uniform(-math.pi,math.pi)
    radius=rng.uniform(10500,15700)
    scale=rng.uniform(.65,1.25)
    mesh_actor(trees[index%2],f"PM_Tree_{index:02d}",centre+unreal.Vector(math.cos(angle)*radius,math.sin(angle)*radius,65),
               unreal.Vector(scale,scale,scale),rng.uniform(0,360))

assert all(transform_values(a)==original[a.get_path_name()] for a in actors if a.get_path_name() in original), "An existing actor was moved"
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
assert unreal.EditorLoadingAndSavingUtils.save_current_level(), "Level save failed"
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(226000,10000,5144),unreal.Rotator(pitch=-45,yaw=0,roll=0))
print(json.dumps({"created":len(created),"stage":str(stage.get_actor_location()),"mountains":5,"landscape":landscape.get_path_name(),"originalActorsUnmoved":True}))
