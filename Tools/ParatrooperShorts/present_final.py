"""Show the finished private set in the editor without saving temporary visibility."""
import unreal

editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=editor.get_editor_world()
assert world.get_path_name().startswith('/Game/RTS_Survival/Maps/Shorts/RTS_Shorts.')
assert not unreal.NavigationSystemV1.is_navigation_being_built(world)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    tags=[str(tag) for tag in actor.tags]
    if any(tag.startswith('ShortsOnly_') for tag in tags):
        actor.set_is_temporarily_hidden_in_editor('ShortsOnly_ParatrooperMeadow' not in tags)
    if isinstance(actor,unreal.DirectionalLight):
        actor.set_is_temporarily_hidden_in_editor(actor.get_actor_label()!='ParatrooperMeadow_Sun')
editor.set_level_viewport_camera_info(unreal.Vector(226400,10350,5350),unreal.Rotator(pitch=-49,yaw=-5.5,roll=0))
print('Finished paratrooper set saved and displayed; other stage visibility is temporary.')
