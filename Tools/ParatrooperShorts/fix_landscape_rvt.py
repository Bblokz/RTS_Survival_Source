"""Feed the existing pit RVT from the private meadow and leave room for its new infantry lines."""
import unreal

editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=editor.get_editor_world()
assert world.get_path_name().startswith('/Game/RTS_Survival/Maps/Shorts/RTS_Shorts.')
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=subsystem.get_all_level_actors()
landscape=next(actor for actor in actors if actor.get_actor_label()=='PM_Landscape')
material=landscape.get_editor_property('landscape_material')
assert material.get_name()=='M_ParatrooperMeadowDiverseGround'
rvt=unreal.load_asset('/Game/RTS_Survival/Meshes/RVTs/RVT_Environment')
assert rvt
library=unreal.MaterialEditingLibrary
output=unreal.find_object(material,'MaterialExpressionRuntimeVirtualTextureOutput_0')
if output is None:
    colour=library.get_material_property_input_node(material,unreal.MaterialProperty.MP_BASE_COLOR)
    normal=library.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
    roughness=library.get_material_property_input_node(material,unreal.MaterialProperty.MP_ROUGHNESS)
    world_normal=library.create_material_expression(material,unreal.MaterialExpressionTransform,1100,1100)
    world_normal.set_editor_property('transform_source_type',unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT)
    world_normal.set_editor_property('transform_type',unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    assert library.connect_material_expressions(normal,'',world_normal,'')
    specular=library.create_material_expression(material,unreal.MaterialExpressionConstant,1100,1300)
    specular.set_editor_property('r',.5)
    opacity=library.create_material_expression(material,unreal.MaterialExpressionConstant,1100,1450)
    opacity.set_editor_property('r',1.)
    output=library.create_material_expression(material,unreal.MaterialExpressionRuntimeVirtualTextureOutput,1500,1000)
    output.set_editor_property('desc','Meadow colour and world normal for the RTS pit ground material')
    pins={name.replace(' ','').lower():name for name in library.get_material_expression_input_names(output)}
    for source,name in [(colour,'basecolor'),(world_normal,'normal'),(roughness,'roughness'),(specular,'specular'),(opacity,'opacity')]:
        assert library.connect_material_expressions(source,'',output,pins[name]),name
    library.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)

textures=list(landscape.get_editor_property('runtime_virtual_textures'))
if rvt not in textures:
    landscape.set_editor_property('runtime_virtual_textures',textures+[rvt])
landscape.set_editor_property('virtual_texture_render_pass_type',unreal.RuntimeVirtualTextureMainPassType.ALWAYS)
# Editor scripting does not propagate the proxy's writer array into existing landscape render components.
for landscape_component in landscape.get_components_by_class(unreal.LandscapeComponent):
    component_textures=list(landscape_component.get_editor_property('runtime_virtual_textures'))
    if rvt not in component_textures:
        landscape_component.set_editor_property('runtime_virtual_textures',component_textures+[rvt])
    landscape_component.set_editor_property('virtual_texture_render_pass_type',unreal.RuntimeVirtualTextureMainPassType.ALWAYS)
volume=next((actor for actor in actors if actor.get_actor_label()=='PM_RVT_Environment'),None)
if volume is None:
    volume=subsystem.spawn_actor_from_class(unreal.RuntimeVirtualTextureVolume,unreal.Vector(211100,-8900,-1000))
volume.set_actor_label('PM_RVT_Environment')
volume.set_folder_path('Shorts/ParatrooperMeadow/RVT')
volume.set_editor_property('tags',[unreal.Name('ShortsOnly_ParatrooperMeadow')])
volume.set_actor_location(unreal.Vector(211100,-8900,-1000),False,False)
# The component uses a unit cube from (0,0,0) to (1,1,1), so its scale is the full coverage in centimetres.
volume.set_actor_scale3d(unreal.Vector(37800,37800,8000))
component=volume.get_component_by_class(unreal.RuntimeVirtualTextureComponent)
component.set_editor_property('virtual_texture',rvt)
component.set_editor_property('bounds_align_actor',landscape)
component.set_editor_property('hide_primitives',False)

# Bring each complete U-shaped defence forward to make a clear infantry corridor ahead of the tank hulls.
for side,front,returns,corner,advance in [('Soviet',-1350,[-1750,-2350,-2950],-1430,550),('German',1100,[1500,2100,2700],1270,-550)]:
    original_x=[front]*5+returns+[corner]+returns+[corner]
    for index,local_x in enumerate(original_x):
        actor=next(actor for actor in actors if actor.get_actor_label()==f'PM_Defence_{side}_{index:02d}')
        point=actor.get_actor_location()
        point.x=230000+local_x+advance
        actor.set_actor_location(point,False,False)

assert unreal.EditorLoadingAndSavingUtils.save_current_level()
print('Saved meadow RVT output and volume for RVT_Environment, plus infantry corridors behind both sandbag fronts.')
print('RVT output inputs:',library.get_material_expression_input_names(output))
print('Landscape writers:',landscape.get_editor_property('runtime_virtual_textures'))
print('RVT coverage:',volume.get_actor_location(),volume.get_actor_scale3d())
