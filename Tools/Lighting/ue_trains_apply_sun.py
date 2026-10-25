import unreal

EditorWorld = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert EditorWorld.get_name() == 'Ger_Trains'
Actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
Sun = next(Actor for Actor in Actors if isinstance(Actor, unreal.DirectionalLight))
SunComponent = Sun.get_component_by_class(unreal.DirectionalLightComponent)
LightFunction = unreal.load_asset('/Game/RTS_Survival/Materials/Sky/M_LF_TrainsSunbreaks')
SunComponent.set_light_function_material(LightFunction)
SunComponent.set_editor_property('light_function_fade_distance', 150000.0)
SunComponent.set_editor_property('disabled_brightness', 0.42)
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([])
print('SUN_ASSIGNED', Sun.get_actor_label(), LightFunction.get_path_name())
print('SUN_RETAINED', SunComponent.get_editor_property('intensity'), SunComponent.get_editor_property('light_color'))
