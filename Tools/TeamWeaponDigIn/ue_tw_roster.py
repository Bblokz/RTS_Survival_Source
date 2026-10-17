"""Reads (and with APPLY = True fixes) the team weapon squad and gun Blueprints' class defaults: the squad and gun
`Squad Subtype`, the gun's `Weapon Name`, the squad's TeamWeaponClass, soldier classes and owning player.
Run through the editor bridge (execute_python) or a -run=pythonscript commandlet. FIXES below is the intended setup."""
import unreal

ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/'
SQUADS = ['Pak38_50mm/Ger_SquadTW_Pak38', 'Pak40_75MM/Ger_SquadTW_Pak40_75MM', 'Flak37mm/Ger_SquadTW_Flak37MM',
          'Flak88Wheels/Ger_SquadTW_Flak88', 'WheeledMortar/Ger_SquadTW_80mmMortar', 'LefhHowitzer/Ger_SquadTW_Lefh',
          'Morser210MM/Ger_SquadTW_Morser210', 'Soviet/53K_45MM/Rus_TWSquad_53K_45mm',
          'Soviet/53K_45MM/Rus_TWSquad_53K_45mm_Soviet', 'Soviet/Zis_76MM/Rus_TWSquad_Zis3_76',
          'Soviet/Zis_76MM/Rus_TWSquad_Zis3_76_Soviet', 'Soviet/BS3_100MM/Rus_TWSquad_BS3_100mm',
          'Soviet/MaximMG/Rus_TWSquad_Maxim', 'Soviet/MaximMG/Rus_TWSquad_Maxim_Soviet',
          'Soviet/80MMMortar/Rus_SquadTW_80mmMortar', 'Soviet/80MMMortar/Rus_SquadTW_80mmMortar_Soviet',
          'Soviet/120Mortar/Rus_SquadTW_120mmMortar', 'Soviet/120Mortar/Rus_SquadTW_120mmMortar_Soviet',
          'Soviet/SovietHowitzer/Rus_SquadTW_M1938', 'Soviet/SovietHowitzer/Rus_SquadTW_M1938_Soviet',
          'Soviet/ML20_152MM/Rus_SquadTW_ML20', 'Soviet/B4_203MM/Rus_SquadTW_B4', 'Soviet/B4_203MM/Rus_SquadTW_B4_Soviet']
GUNS = ['Pak38_50mm/Ger_TW_Pak38', 'Pak40_75MM/Ger_TW_Pak40', 'Flak37mm/Ger_TW_Flak37MM', 'Flak88Wheels/Ger_TW_Flak88',
        'WheeledMortar/BP_TW_Ger80MM_Mortar', 'LefhHowitzer/BP_TW_Lefh', 'LFH18/BP_TW_Lefh18', 'Morser210MM/BP_TW_Morser',
        'Soviet/53K_45MM/Rus_TW_53K_45mm', 'Soviet/53K_45MM/Rus_TW_53K_45mm_Soviet', 'Soviet/Zis_76MM/Rus_TW_Zis3_76',
        'Soviet/Zis_76MM/Rus_TW_Zis3_76_Soviet', 'Soviet/BS3_100MM/Rus_TW_BS3_100mm', 'Soviet/MaximMG/Rus_TW_Maxim',
        'Soviet/MaximMG/Rus_TW_Maxim_Soviet', 'Soviet/80MMMortar/BP_TW_Rus80MM_Mortar',
        'Soviet/80MMMortar/BP_TW_Rus80MM_Mortar_Soviet', 'Soviet/120Mortar/BP_TW_Rus120MM_Mortar',
        'Soviet/120Mortar/BP_TW_Rus120MM_Mortar_Soviet', 'Soviet/SovietHowitzer/BP_TW_M1938',
        'Soviet/SovietHowitzer/BP_TW_M1938_Soviet', 'Soviet/ML20_152MM/BP_TW_ML20', 'Soviet/B4_203MM/BP_TW_B4',
        'Soviet/B4_203MM/BP_TW_B4_Soviet']


def cdo_of(path):
    bp = unreal.load_asset(ROOT + path)
    if bp is None:
        return None, None
    return bp, unreal.get_default_object(bp.generated_class())


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception as error:
        return f'<{type(error).__name__}>'


def rts_templates(bp):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(template, unreal.RTSComponent):
            result.append(template)
    return result


def describe():
    for path in SQUADS:
        bp, cdo = cdo_of(path)
        if cdo is None:
            print('ROSTER missing', path)
            continue
        owners = [t.get_editor_property('owning_player') for t in rts_templates(bp)]
        units = {}
        for unit_class in prop(cdo, 'squad_unit_classes') or []:
            name = unit_class.get_name() if hasattr(unit_class, 'get_name') else str(unit_class)
            units[name] = units.get(name, 0) + 1
        weapon_class = prop(cdo, 'team_weapon_class')
        print('ROSTER squad', path.split('/')[-1], 'subtype', prop(cdo, 'Squad Subtype'), 'owner', owners,
              'gun', weapon_class.get_name() if hasattr(weapon_class, 'get_name') else weapon_class, 'units', units)
    for path in GUNS:
        bp, cdo = cdo_of(path)
        if cdo is None:
            print('ROSTER missing', path)
            continue
        owners = [t.get_editor_property('owning_player') for t in rts_templates(bp)]
        print('ROSTER gun', path.split('/')[-1], 'subtype', prop(cdo, 'Squad Subtype'), 'weapon',
              prop(cdo, 'Weapon Name'), 'owner', owners)


describe()
