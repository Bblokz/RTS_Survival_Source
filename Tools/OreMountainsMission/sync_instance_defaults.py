"""Copy chosen stage-default values onto the stage instances in Ger_2ndOreMountains' BP_MissionManager (instances
placed before a default changed can keep the old value), then save the level. Sent over UECP by
run_sync_instance_defaults (editor Python only, no os)."""
import unreal

PROPS = ['StartLine', 'StoppedLine', 'EscapedLine', 'FirstCaptureLine', 'SealedCargoLine', 'UnmarkedLine', 'WarningLine',
         'DoneLine', 'ObjectiveLine', 'RecoveredLine', 'BerlinLine', 'FuelWarningLine', 'AmmoWarningLine',
         'PartsWarningLine', 'ConcreteWarningLine', 'ProgressPrefix', 'TargetTag', 'RouteId', 'BatteryId',
         'RadioOutpostStage', 'RecoverCasesStage', 'settings', 'EscapedCasesTransform', 'EscapeBonusSpawn',
         'EnemyWaveSupply', 'ConvoyApproachLine1', 'ConvoyApproachLine2', 'ConvoyApproachLine3']
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
manager = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
           if a.get_class().get_name() == 'BP_MissionManager_C'][0]
manager.modify()
changed = []
for mission in manager.get_editor_property('missions'):
    if mission is None:
        continue
    cdo = unreal.get_default_object(mission.get_class())
    for prop in PROPS:
        try:
            want = cdo.get_editor_property(prop)
        except Exception:
            continue
        if mission.get_editor_property(prop) != want:
            mission.set_editor_property(prop, want)
            changed.append(f'{mission.get_class().get_name()}.{prop}')
print('CHANGED', changed)
print('SAVED', unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())
