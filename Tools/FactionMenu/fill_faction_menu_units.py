"""Fills the per-faction unit lists of BP_W_FactionSelectionMenu (FFactionMenuSetting on the C++ UW_FactionSelectionMenu).

The lists follow the design's faction roster (mechanized depot = recon, barracks support weapons, light / medium /
heavy factories); both German factions share the same infantry. Aircraft and building expansions are left as set.
Run as a commandlet: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>.
"""
import unreal

MENU = "/Game/RTS_Survival/FactionSelection/UI/BP_W_FactionSelectionMenu"

SHARED_INFANTRY = [
    "Squad_Ger_Scavengers", "Squad_Ger_JagerTruppKar98k", "Squad_Ger_SturmPionieren", "Squad_Ger_Vultures",
    "Squad_Ger_SturmKommandos", "Squad_Ger_Gebirgsjagerin", "Squad_Ger_SteelFistAssaultSquad",
    "Squad_Ger_PanzerGrenadiere", "Squad_Ger_SniperTeam", "Squad_Ger_LightBringers",
]

FACTIONS = {
    "m_ger_breakthrough_setting": {
        "armored_cars": ["Tank_Sdkfz250", "Tank_Pz38t", "Tank_Sdkfz_221", "Tank_Sdkfz250_37mm"],
        "light_tanks": ["Tank_PzII_Luchs", "Tank_Sdkfz_7_Flakvierling", "Tank_PzJager", "Tank_PzI_15cm",
                        "Tank_PzIV_F1", "Tank_Pz38t_RailGun"],
        "medium_tanks": ["Tank_Marder", "Tank_PzIV_G", "Tank_PzIV_70", "Tank_Brumbar", "Tank_PzIV_H",
                         "Tank_PZIV_Rockets"],
        "heavy_tanks": ["Tank_E25", "Tank_TigerH1", "Tank_TigerRail", "Tank_SturmTiger", "Tank_KingTiger",
                        "Tank_Tiger105", "Tank_Maus"],
        "support_weapons": ["Squad_Ger_PaK38", "Squad_Ger_LefH18", "Squad_Ger_PaK40", "Squad_Ger_210MM_Morser"],
    },
    "m_ger_thermo_korps_setting": {
        "armored_cars": ["Tank_Sdkfz251_Transport", "Tank_Sdkfz_140", "Tank_Sdkfz_221", "Tank_Sdkfz251_Mortar",
                         "Tank_Sdkfz251_22"],
        "light_tanks": ["Tank_PzII_F", "Tank_Puma", "Tank_PzJagerLaser", "Tank_Sdkfz_232_3", "Tank_PzIII_J",
                        "Tank_PzIII_AA", "Tank_Pz38t_R"],
        "medium_tanks": ["Tank_Stug", "Tank_Jaguar", "Tank_Hetzer", "Tank_Wespe", "Tank_PzIII_M",
                         "Tank_PzIII_FLamm"],
        "heavy_tanks": ["Tank_E25", "Tank_PantherD", "Tank_JagdPanther", "Tank_PanzerV_III", "Tank_SturmTiger",
                        "Tank_PantherII", "Tank_E100"],
        "support_weapons": ["Squad_Ger_PaK38", "Squad_Ger_SFH18_150mm", "Squad_Ger_88mmFlak",
                            "Squad_Ger_210MM_Morser"],
    },
}


def normalise(name):
    return name.replace("_", "").upper()


def enum_lookup(enum_type):
    # Python renames C++ enumerators (Tank_PzIV_F1 -> TANK_PZ_IV_F1); match them with the underscores dropped.
    return {normalise(member.name): member for member in enum_type}


def to_enum(lookup, cpp_name):
    member = lookup.get(normalise(cpp_name))
    if member is None:
        raise RuntimeError("FACTION_MENU_FILL unknown enumerator " + cpp_name)
    return member


def tank_options(lookup, names):
    options = []
    for name in names:
        option = unreal.FactionMenuTankOption()
        option.set_editor_property("tank_subtype", to_enum(lookup, name))
        options.append(option)
    return options


def squad_options(lookup, names):
    options = []
    for name in names:
        option = unreal.FactionMenuSquadOption()
        option.set_editor_property("squad_subtype", to_enum(lookup, name))
        options.append(option)
    return options


def main():
    tanks = enum_lookup(unreal.TankSubtype)
    squads = enum_lookup(unreal.SquadSubtype)
    menu = unreal.load_asset(MENU)
    menu_cdo = unreal.get_default_object(menu.generated_class())
    for property_name, roster in FACTIONS.items():
        setting = menu_cdo.get_editor_property(property_name)
        for group in ("armored_cars", "light_tanks", "medium_tanks", "heavy_tanks"):
            setting.set_editor_property(group, tank_options(tanks, roster[group]))
        setting.set_editor_property("infantry", squad_options(squads, SHARED_INFANTRY))
        setting.set_editor_property("support_weapons", squad_options(squads, roster["support_weapons"]))
        menu_cdo.set_editor_property(property_name, setting)
    unreal.BlueprintEditorLibrary.compile_blueprint(menu)
    unreal.EditorAssetLibrary.save_asset(MENU, only_if_is_dirty=False)

    saved = unreal.get_default_object(unreal.load_asset(MENU).generated_class())
    for property_name in FACTIONS:
        setting = saved.get_editor_property(property_name)
        for group in ("armored_cars", "light_tanks", "medium_tanks", "heavy_tanks"):
            names = [str(option.get_editor_property("tank_subtype")).split(".")[-1].split(":")[0]
                     for option in setting.get_editor_property(group)]
            unreal.log("FACTION_MENU_FILL %s %s %s" % (property_name, group, names))
        for group in ("infantry", "support_weapons", "aircraft"):
            unreal.log("FACTION_MENU_FILL %s %s count=%d" % (property_name, group,
                                                        len(setting.get_editor_property(group))))
    unreal.log("FACTION_MENU_FILL done")


main()
