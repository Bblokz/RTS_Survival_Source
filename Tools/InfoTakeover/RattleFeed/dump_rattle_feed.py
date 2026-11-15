"""Prints the Rattle-Feed (RapidFireAutoCannon behaviour ability) settings of the German autocannon vehicles and the
BP_Beh_RapidFire_Autocannon behaviour values, so the takeover text matches the data. Read-only; run headless:
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
-DisablePlugins=Fab,BpGeneratorUltimate
Grep RATTLE_FEED_DUMP.
"""
import unreal

VEHICLES = [
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/PanzerII/Ger_Vehicle_PanzerII_F',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz231/Ger_ArmoredCar_Sdkfz_231',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/SDKFZ-140/Ger_Vehicle_SDKFZ-140',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/Jaguar/Ger_Vehicle_Jaguar',
]
BEHAVIOUR = '/Game/RTS_Survival/Blueprints/Behaviours/Weapon/BP_Beh_RapidFire_Autocannon'


def log(message):
    unreal.log('RATTLE_FEED_DUMP ' + message)


def dump_vehicle(path):
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        log('%s: not found' % path)
        return
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template is None or not isinstance(template, unreal.ApplyBehaviourAbilityComponent):
            continue
        settings = template.get_editor_property('behaviour_ability_settings')
        log('%s: %s ability=%s cooldown=%s behaviour=%s slot=%s' % (
            path.rsplit('/', 1)[1], template.get_name(), settings.get_editor_property('behaviour_ability'),
            settings.get_editor_property('cooldown'), settings.get_editor_property('behaviour_applied'),
            settings.get_editor_property('preferred_ability_index')))


def dump_behaviour():
    generated = unreal.load_object(None, BEHAVIOUR + '.' + BEHAVIOUR.rsplit('/', 1)[1] + '_C')
    cdo = unreal.get_default_object(generated)
    log('behaviour export: ' + str(cdo.export_text()) if hasattr(cdo, 'export_text') else 'no export_text')
    for name in ['behaviour_weapon_multipliers', 'm_life_time_duration', 'minimal_weapon_calibre_to_apply_to']:
        try:
            log('behaviour %s = %s' % (name, cdo.get_editor_property(name)))
        except Exception as error:
            log('behaviour %s unreadable: %s' % (name, error))


for vehicle in VEHICLES:
    dump_vehicle(vehicle)
dump_behaviour()
