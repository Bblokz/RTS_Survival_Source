"""-ExecutePythonScript on Ger_IntroMission: prints what loaded from disk (Blueprint CDO Settings, Missions[3]). """
import sys

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission')
import rewire_ridge_chain  # noqa: E402

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
bp = unreal.load_asset(rewire_ridge_chain.MISSION_DIR + rewire_ridge_chain.COUNTERATTACK_BP)
cdo = unreal.get_default_object(bp.generated_class())
unreal.log_warning('COUNTERVERIFY cdo settings ' + str(cdo.get_editor_property('settings')))
manager = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
           if a.get_class().get_name() == 'BP_MissionManager_C'][0]
missions = manager.get_editor_property('missions')
counter = missions[3] if len(missions) > 3 else None
unreal.log_warning('COUNTERVERIFY instance ' + str(counter) + ' settings '
                   + str(counter.get_editor_property('settings') if counter else None))
da = counter.get_editor_property('settings') if counter else None
if da:
    train = da.get_editor_property('train')
    unreal.log_warning('COUNTERVERIFY train class %s path %d meshes %d timer %s' % (
        train.get_editor_property('train_class'), len(train.get_editor_property('path_points')),
        len(train.get_editor_property('cargo_tank_meshes')), da.get_editor_property('timer_widget_class')))
for line in rewire_ridge_chain.describe(world):
    unreal.log_warning('COUNTERVERIFY ' + line)
