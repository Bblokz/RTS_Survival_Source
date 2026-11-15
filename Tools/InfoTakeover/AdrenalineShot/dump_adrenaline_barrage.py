"""Prints the Adrenaline Shot (Panzer 38(t)) and Wespe Barrage ability settings and their behaviour values, so the
takeover text matches the data. Read-only; run headless:
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
-DisablePlugins=Fab,BpGeneratorUltimate
Grep ADRENALINE_BARRAGE_DUMP.
"""
import unreal

VEHICLES = [
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Wespe/Ger_Vehicle_Wespe',
]
BEHAVIOURS = [
    '/Game/RTS_Survival/Blueprints/Behaviours/WeaponAndVFX/Beh_WeaponVFX_Pz38AdrenalineShot',
    '/Game/RTS_Survival/Blueprints/Behaviours/ForAimAbility/Beh_Aim_WespeBarrage',
]
SETTING_NAMES = {
    'ApplyBehaviourAbilityComponent': 'behaviour_ability_settings',
    'AimAbilityComponent': 'm_aim_ability_settings',
}


def log(message):
    unreal.log('ADRENALINE_BARRAGE_DUMP ' + message)


def describe_struct(struct):
    parts = []
    for name in ['behaviour_ability', 'aim_ability_type', 'cooldown', 'behaviour_duration', 'behaviour_applied',
                 'abilities_to_remove', 'range_percentage', 'radius', 'preferred_ability_index']:
        try:
            parts.append('%s=%s' % (name, struct.get_editor_property(name)))
        except Exception:
            pass
    return ', '.join(parts)


def dump_vehicle(path):
    blueprint = unreal.load_asset(path)
    if blueprint is None:
        log('%s: not found' % path)
        return
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template is None:
            continue
        for class_name, setting_name in SETTING_NAMES.items():
            component_class = getattr(unreal, class_name, None)
            if component_class is None or not isinstance(template, component_class):
                continue
            try:
                settings = template.get_editor_property(setting_name)
            except Exception as error:
                log('%s: %s settings unreadable: %s' % (path.rsplit('/', 1)[1], template.get_name(), error))
                continue
            log('%s: %s (%s) %s' % (path.rsplit('/', 1)[1], template.get_name(), template.get_class().get_name(),
                                    describe_struct(settings)))


def dump_behaviour(path):
    generated = unreal.load_object(None, path + '.' + path.rsplit('/', 1)[1] + '_C')
    cdo = unreal.get_default_object(generated)
    for name in ['behaviour_weapon_multipliers', 'behaviour_weapon_attributes', 'm_life_time_duration',
                 'minimal_weapon_calibre_to_apply_to']:
        try:
            log('%s %s = %s' % (path.rsplit('/', 1)[1], name, cdo.get_editor_property(name)))
        except Exception as error:
            log('%s %s unreadable: %s' % (path.rsplit('/', 1)[1], name, error))


for vehicle in VEHICLES:
    dump_vehicle(vehicle)
for behaviour in BEHAVIOURS:
    dump_behaviour(behaviour)
