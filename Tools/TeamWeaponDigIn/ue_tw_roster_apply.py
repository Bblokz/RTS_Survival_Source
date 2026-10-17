"""Applies the team weapon class-default fixes (weapons, the German 80 mm mortar squad's gun and soldiers), then
compiles and saves every Blueprint touched by the roster fix, including those whose construction-script subtype
literal was changed through the bridge (set_pin_default)."""
import unreal

ROOT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/'


def weapon(fragment):
    # UE Python's upper-snake spelling of mixed-case enum names is irregular (sFH18 -> S_FH18); match on a fragment.
    return [getattr(unreal.WeaponName, name) for name in dir(unreal.WeaponName) if name.endswith(fragment)][0]


WEAPONS = {
    'Soviet/Zis_76MM/Rus_TW_Zis3_76': weapon('ZIS_3_76MM_TW'),
    'LefhHowitzer/BP_TW_Lefh': weapon('FH_18_105MM_TW'),
    'LFH18/BP_TW_Lefh18': weapon('FH18_150MM_TW'),
    'Morser210MM/BP_TW_Morser': weapon('MORSER_18_210MM_TW'),
    'Soviet/MaximMG/Rus_TW_Maxim': weapon('M1910_7_62MM_TW'),
    'Soviet/80MMMortar/BP_TW_Rus80MM_Mortar': weapon('MORTAR_80MM_TW'),
}
GERMAN_RIFLEMAN = '/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/JagerTrupp/Ger_Inf_Kar.Ger_Inf_Kar_C'
GER_MORTAR_SQUAD = 'WheeledMortar/Ger_SquadTW_80mmMortar'
GER_MORTAR_GUN = ROOT + 'WheeledMortar/BP_TW_Ger80MM_Mortar.BP_TW_Ger80MM_Mortar_C'
SUBTYPE_EDITED = ['WheeledMortar/Ger_SquadTW_80mmMortar', 'Morser210MM/Ger_SquadTW_Morser210',
                  'Soviet/53K_45MM/Rus_TWSquad_53K_45mm', 'Soviet/BS3_100MM/Rus_TWSquad_BS3_100mm',
                  'Soviet/80MMMortar/Rus_SquadTW_80mmMortar', 'Soviet/ML20_152MM/Rus_SquadTW_ML20',
                  'Flak37mm/Ger_TW_Flak37MM', 'Soviet/53K_45MM/Rus_TW_53K_45mm', 'Soviet/BS3_100MM/Rus_TW_BS3_100mm',
                  'Soviet/MaximMG/Rus_TW_Maxim', 'Soviet/80MMMortar/BP_TW_Rus80MM_Mortar', 'Morser210MM/BP_TW_Morser',
                  'Soviet/ML20_152MM/BP_TW_ML20', 'LFH18/BP_TW_Lefh18', 'WheeledMortar/BP_TW_Ger80MM_Mortar']
# Faction copies inherit from the originals; recompiled so their generated classes pick up the change.
CHILDREN = ['Soviet/53K_45MM/Rus_TWSquad_53K_45mm_Soviet', 'Soviet/80MMMortar/Rus_SquadTW_80mmMortar_Soviet',
            'Soviet/53K_45MM/Rus_TW_53K_45mm_Soviet', 'Soviet/MaximMG/Rus_TW_Maxim_Soviet',
            'Soviet/80MMMortar/BP_TW_Rus80MM_Mortar_Soviet', 'Soviet/Zis_76MM/Rus_TW_Zis3_76_Soviet']

touched = []
for path, weapon_name in WEAPONS.items():
    bp = unreal.load_asset(ROOT + path)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.modify()
    cdo.set_editor_property('Weapon Name', weapon_name)
    touched.append(path)

squad_bp = unreal.load_asset(ROOT + GER_MORTAR_SQUAD)
squad_cdo = unreal.get_default_object(squad_bp.generated_class())
squad_cdo.modify()
squad_cdo.set_editor_property('team_weapon_class', unreal.load_class(None, GER_MORTAR_GUN))
units = squad_cdo.get_editor_property('squad_unit_classes')
squad_cdo.set_editor_property('squad_unit_classes',
                              [unreal.load_class(None, GERMAN_RIFLEMAN) for _ in range(len(units))])

packages = []
for path in list(dict.fromkeys(touched + SUBTYPE_EDITED + [GER_MORTAR_SQUAD] + CHILDREN)):
    bp = unreal.load_asset(ROOT + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    packages.append(bp.get_outermost())
saved = unreal.EditorLoadingAndSavingUtils.save_packages(packages, only_dirty=False)
print('ROSTER_APPLY saved', saved, len(packages))

for path in WEAPONS:
    cdo = unreal.get_default_object(unreal.load_asset(ROOT + path).generated_class())
    print('ROSTER_APPLY weapon', path.split('/')[-1], cdo.get_editor_property('Weapon Name'))
cdo = unreal.get_default_object(unreal.load_asset(ROOT + GER_MORTAR_SQUAD).generated_class())
print('ROSTER_APPLY ger mortar gun', cdo.get_editor_property('team_weapon_class').get_name(), 'units',
      [c.get_name() for c in cdo.get_editor_property('squad_unit_classes')])
