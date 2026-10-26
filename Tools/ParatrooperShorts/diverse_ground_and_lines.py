"""Blend meadow, dry grass, mud and stone; replace scattered bags with facing defensive fronts."""
import math
import random
import unreal

subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith("/Game/RTS_Survival/Maps/Shorts/RTS_Shorts.")
actors=subsystem.get_all_level_actors()
tag=unreal.Name("ShortsOnly_ParatrooperMeadow")
landscape=next(a for a in actors if a.get_actor_label()=="PM_Landscape")

task=unreal.AssetImportTask()
task.set_editor_property("filename","D:/UE5Projects/RTS_Survival/Source/Tools/ParatrooperShorts/ground_blend_weights.png")
task.set_editor_property("destination_path","/Game/RTS_Survival/Shorts")
task.set_editor_property("destination_name","T_ParatrooperGroundBlend")
task.set_editor_property("automated",True)
task.set_editor_property("replace_existing",True)
task.set_editor_property("save",False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mask_texture=unreal.load_asset("/Game/RTS_Survival/Shorts/T_ParatrooperGroundBlend")
assert mask_texture
mask_texture.set_editor_property("srgb",False)
mask_texture.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_MASKS)
mask_texture.set_editor_property("address_x",unreal.TextureAddress.TA_CLAMP)
mask_texture.set_editor_property("address_y",unreal.TextureAddress.TA_CLAMP)
unreal.EditorAssetLibrary.save_loaded_asset(mask_texture,False)

material=unreal.load_asset("/Game/RTS_Survival/Shorts/M_ParatrooperMeadowDiverseGround")
if not material:
    material=unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_ParatrooperMeadowDiverseGround","/Game/RTS_Survival/Shorts",unreal.Material,unreal.MaterialFactoryNew())
library=unreal.MaterialEditingLibrary
for attempt in range(12):
    if library.get_num_material_expressions(material)==0:
        break
    library.delete_all_material_expressions(material)
assert library.get_num_material_expressions(material)==0
node_index=0
def node(cls):
    global node_index
    result=library.create_material_expression(material,cls,(node_index%6)*220,(node_index//6)*220)
    node_index+=1
    return result
def wire(source,pin,target,input_name):
    assert library.connect_material_expressions(source,pin,target,input_name),(source,pin,target,input_name)
def scalar(value):
    result=node(unreal.MaterialExpressionConstant)
    result.set_editor_property("r",value)
    return result
position=node(unreal.MaterialExpressionWorldPosition)
xy=node(unreal.MaterialExpressionComponentMask)
xy.set_editor_property("r",True)
xy.set_editor_property("g",True)
wire(position,"",xy,"")
tile=node(unreal.MaterialExpressionMultiply)
wire(xy,"",tile,"A")
wire(scalar(.0025),"",tile,"B")
origin=node(unreal.MaterialExpressionConstant2Vector)
origin.set_editor_property("r",211100)
origin.set_editor_property("g",-8900)
relative=node(unreal.MaterialExpressionSubtract)
wire(xy,"",relative,"A")
wire(origin,"",relative,"B")
mask_uv=node(unreal.MaterialExpressionMultiply)
wire(relative,"",mask_uv,"A")
wire(scalar(1/37800),"",mask_uv,"B")
weights=node(unreal.MaterialExpressionTextureSample)
weights.set_editor_property("texture",mask_texture)
weights.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
wire(mask_uv,"",weights,"UVs")
def texture(path,normal=False):
    result=node(unreal.MaterialExpressionTextureSample)
    result.set_editor_property("texture",unreal.load_asset(path))
    if normal:
        result.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    wire(tile,"",result,"UVs")
    return result
root="/Game/Brushify/Materials/Landscape/Textures/"
airport="/Game/MilitaryAirport/Landscape/LandscapeTextures/"
grass=texture(airport+"T_Grass_a_BaseColor_Height")
dry=texture(root+"Grass/T_GrassDry_D")
mud=texture(root+"Mud/T_Mud_D")
rock=texture(airport+"T_Ground_Rocks_BaseColor_Height")
def tint(source,colour):
    value=node(unreal.MaterialExpressionConstant3Vector)
    value.set_editor_property("constant",unreal.LinearColor(*colour,1))
    result=node(unreal.MaterialExpressionMultiply)
    wire(source,"RGB",result,"A")
    wire(value,"",result,"B")
    return result
dry=tint(dry,(.9,.78,.58))
mud=tint(mud,(.34,.24,.15))
rock=tint(rock,(.68,.64,.57))
def blend(first,second,channel):
    result=node(unreal.MaterialExpressionLinearInterpolate)
    wire(first,"RGB" if isinstance(first,unreal.MaterialExpressionTextureSample) else "",result,"A")
    wire(second,"RGB" if isinstance(second,unreal.MaterialExpressionTextureSample) else "",result,"B")
    wire(weights,channel,result,"Alpha")
    return result
colours=blend(blend(blend(grass,dry,"G"),mud,"R"),rock,"B")
assert library.connect_material_property(colours,"",unreal.MaterialProperty.MP_BASE_COLOR)
grass_normal=texture(airport+"T_Grass_a_Normal",True)
dry_normal=texture(root+"Grass/T_GrassDry_N",True)
mud_normal=texture(root+"Mud/T_Mud_N",True)
normal=blend(blend(grass_normal,dry_normal,"G"),mud_normal,"R")
assert library.connect_material_property(normal,"",unreal.MaterialProperty.MP_NORMAL)
assert library.connect_material_property(scalar(.94),"",unreal.MaterialProperty.MP_ROUGHNESS)
library.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material,False)
landscape.set_editor_property("landscape_material",material)

for actor in actors:
    if actor.get_actor_label().startswith(("PM_Dress_SovietSandbags_","PM_Dress_GermanSandbags_","PM_Defence_","PM_GroundDetail_")):
        subsystem.destroy_actor(actor)
ignored_ground=[a for a in subsystem.get_all_level_actors() if not isinstance(a,unreal.LandscapeProxy)]
def ground(x,y):
    hit=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,3000),unreal.Vector(x,y,-500),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored_ground,unreal.DrawDebugTrace.NONE)
    assert hit,(x,y)
    assert isinstance(hit.to_tuple()[9],unreal.LandscapeProxy),(x,y,hit.to_tuple()[9])
    return hit.to_tuple()[5]
def mark(actor,label,folder):
    actor.set_actor_label(label)
    actor.set_folder_path("Shorts/ParatrooperMeadow/"+folder)
    actor.set_editor_property("tags",[tag])
    return actor
def sandbag(side,index,x,y,yaw,scale):
    path=("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/SandBagWalls/BP_Cover_RusSandbags_DoubleStraight" if side=="Soviet" else "/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Ger_SandBagWall_FieldConstruction")
    cls=unreal.load_class(None,path+"."+path.rsplit("/",1)[1]+"_C")
    infantry_corridor_advance=550 if side=="Soviet" else -550
    actor=mark(subsystem.spawn_actor_from_class(cls,ground(230000+x+infantry_corridor_advance,10000+y),unreal.Rotator(pitch=0,yaw=yaw,roll=0)),f"PM_Defence_{side}_{index:02d}","DefensiveLines/"+side)
    actor.set_actor_scale3d(unreal.Vector(1,scale,1))
    ignored_ground.append(actor)
    rts=actor.get_component_by_class(unreal.RTSComponent)
    if rts:
        rts.set_editor_property("OwningPlayer",2 if side=="Soviet" else 1)

for side in ["Soviet","German"]:
    index=0
    front_x=-1350 if side=="Soviet" else 1100
    front_spacing=900 if side=="Soviet" else 800
    facing=0 if side=="Soviet" else 180
    for slot in [-2,-1,0,1,2]:
        sandbag(side,index,front_x,slot*front_spacing,facing,.9 if side=="Soviet" else .68)
        index+=1
    for flank in [-1,1]:
        flank_y=2750 if side=="Soviet" else 2100
        for x in ([-1750,-2350,-2950] if side=="Soviet" else [1500,2100,2700]):
            sandbag(side,index,x,flank*flank_y,90 if flank>0 else -90,.7)
            index+=1
        sandbag(side,index,-1430 if side=="Soviet" else 1270,flank*(2450 if side=="Soviet" else 1970),flank*45 if side=="Soviet" else 180-flank*45,.52)
        index+=1

# Keep the nearest sheep clear of the new Soviet flank return.
for actor in subsystem.get_all_level_actors():
    if actor.get_actor_label() in ["PM_Animal_Left_Sheep_00","PM_Animal_Right_Sheep_00"]:
        point=actor.get_actor_location()
        point.x=228950
        actor.set_actor_location(point,False,False)

# Scuffed approaches connect the emplacements to the wider meadow.
for index,(x,y,yaw,length,width,name) in enumerate([(-2100,0,0,2200,500,"MI_Decal_Dirt_Tracks_01"),(1700,0,0,2000,500,"MI_Decal_Dirt_Tracks_Large_02"),(-4100,-1700,22,1800,450,"MI_Decal_Dirt_Tracks_Large_01"),(3800,900,-18,1700,500,"MI_Decal_Dirt_Tracks_Large_03")]):
    actor=mark(subsystem.spawn_actor_from_class(unreal.DecalActor,ground(230000+x,10000+y)+unreal.Vector(0,0,100),unreal.Rotator(pitch=-90,yaw=yaw,roll=0)),f"PM_GroundDetail_Tracks_{index}","GroundDetail")
    decal=actor.get_component_by_class(unreal.DecalComponent)
    decal.set_decal_material(unreal.load_asset("/Game/RTS_Survival/Materials/Decals/DirtTracks/"+name))
    decal.set_editor_property("decal_size",unreal.Vector(180,width,length))
    decal.set_editor_property("fade_screen_size",.001)
for path,colour in [("MI_ParatrooperMeadowGrass",unreal.LinearColor(.95,1.0,.87,1)),("MI_ParatrooperMeadowTallGrass",unreal.LinearColor(1.05,.97,.78,1))]:
    instance=unreal.load_asset("/Game/RTS_Survival/Shorts/"+path)
    library.set_material_instance_vector_parameter_value(instance,"Tint",colour)
    unreal.EditorAssetLibrary.save_loaded_asset(instance,False)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
print("Saved varied meadow/dry-grass/mud/stone ground, scuffed approaches and 13 defensive sandbag sections per side.")
