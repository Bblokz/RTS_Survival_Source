"""Apply the meadow stage's final presentation and force-save its level (safe to rerun)."""
import math
import random
import unreal

subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith("/Game/RTS_Survival/Maps/Shorts/RTS_Shorts.")
actors={a.get_actor_label():a for a in subsystem.get_all_level_actors()}

def yaw(label,value):
    actors[label].set_actor_rotation(unreal.Rotator(pitch=0,yaw=value,roll=0),False)

for index,value in enumerate([-15,40,105,155,-45]):
    yaw(f"PM_Mountain_{index:02d}",value)
rng=random.Random(20261025)
for index in range(480):
    local_x=rng.uniform(-10000,12000)
    local_y=rng.uniform(-9000,9000)
    if abs(local_x)<3200 and abs(local_y)<2600:
        continue
    rng.uniform(.8,1.5)
    yaw(f"PM_Grass_{index:03d}",rng.uniform(0,360))
for index in range(36):
    rng.uniform(-math.pi,math.pi)
    rng.uniform(10500,15700)
    rng.uniform(.65,1.25)
    yaw(f"PM_Tree_{index:02d}",rng.uniform(0,360))

for source,label in [("Meadow_Sky","PM_Sky"),("Meadow_Fog","PM_Fog")]:
    if label not in actors:
        actor=subsystem.duplicate_actor(actors[source],world,unreal.Vector(0,0,0))
        actor.set_actor_label(label)
        actor.set_folder_path("Shorts/ParatrooperMeadow")
        actors[label]=actor
    actors[label].set_editor_property("tags",[unreal.Name("ShortsOnly_ParatrooperMeadow")])

material=unreal.load_asset("/Game/RTS_Survival/Shorts/M_ParatrooperMeadowGround")
assert material
actors["PM_Landscape"].set_editor_property("landscape_material",material)
for label in ("PM_Track_0","PM_Track_1"):
    if label in actors:
        subsystem.destroy_actor(actors[label])
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(226000,10000,5144),unreal.Rotator(pitch=-45,yaw=0,roll=0))
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
assert unreal.EditorLoadingAndSavingUtils.save_current_level(), "Level save failed"
print("Saved final meadow atmosphere, textured ground and upright scenery.")
