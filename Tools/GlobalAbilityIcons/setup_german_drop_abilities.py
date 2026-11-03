"""Adds the German vehicle drop templates to DA_GlobalAbilitySystem. Run in the open editor, then import_ability_icons.py.
Durations, sounds and the marker come from the Soviet T-34 drop; costs and cooldown are that template's too and still
need balancing. Each entry names the exact variant class, so the drop is the same whatever the player picked for the
unit type. Re-running replaces the settings of templates that already exist."""
import re

import unreal

DATA_ASSET = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
SOURCE_TEMPLATE = "GA_SOVIET_T34_DROP"
AIRCRAFT = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU390B/Ger_Aircraft_JU390B_AirborneUnarmed.Ger_Aircraft_JU390B_AirborneUnarmed_C"
VEHICLES = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
AIM = "(AimType=SmallCrosshair,AimRadius=1000.000000)"

FLAK41 = ("Tank_PzIII_AA", "Ger_Medium/PanzerIII/Flak41/Ger_Vehicle_PanzerIII_AA_Flak41")
SDKFZ7 = ("Tank_Sdkfz_7_Flakvierling", "Ger_ArmoredCar/Sdkfz7_Flak/Ger_Vehicle_Sdkfz7Flak")
SDKFZ7_FLAK36 = ("Tank_Sdkfz_7_Flakvierling", "Ger_ArmoredCar/Sdkfz7_Flak/Ger_Vehicle_Sdkfz7Flak_Flak36")
PZII_RADIX = ("Tank_PzII_F", "Ger_Light/PanzerII/Ger_Vehicle_PanzerII_F_RadixiteAutocannon")
MARDER_DUAL = ("Tank_Marder", "Ger_Light/Marder1/Ger_Vehicle_Marder_DualBarrel")
FLAK_LUCHS = ("Tank_PzII_Luchs", "Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs_FlakLuchs")
STUG = ("Tank_Stug", "Ger_Medium/Stug/Ger_Vehicle_StugIII")
PZIV_SPOTTER = ("Tank_PzIV_F1", "Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1_Spotter")
PZIII_J = ("Tank_PzIII_J", "Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J")

# enum value name: (object name, title, description, one Ju 390 per vehicle)
DROPS = {
    "GA_GER_PZ_III_FLAK41_DROP": ("GA_AircraftDrop_GerFlak41", "Flak 41 Drop",
                                  "2 Ju 390s land 2 Panzer III Flak 41 anti-air tanks at the target location.",
                                  (FLAK41, FLAK41)),
    "GA_GER_SDKFZ7_FLAK_DROP": ("GA_AircraftDrop_GerSdkfz7Flak", "Sd.Kfz. 7 Flak Drop",
                                "2 Ju 390s land an Sd.Kfz. 7 Flak and an Sd.Kfz. 7 with Flak 36 at the target location.",
                                (SDKFZ7, SDKFZ7_FLAK36)),
    "GA_GER_RADIXITE_PZ_II_MARDER_DROP": ("GA_AircraftDrop_GerRadixiteMarder", "Radixite Pz II and Marder Drop",
                                          "3 Ju 390s land 2 Panzer II F with radixite autocannons and a dual-barrel "
                                          "Marder at the target location.",
                                          (PZII_RADIX, PZII_RADIX, MARDER_DUAL)),
    "GA_GER_FLAK_LUCHS_STUG_DROP": ("GA_AircraftDrop_GerFlakLuchsStug", "Flak Luchs and StuG III Drop",
                                    "3 Ju 390s land 2 Flak Luchs and a StuG III at the target location.",
                                    (FLAK_LUCHS, FLAK_LUCHS, STUG)),
    "GA_GER_SPOTTER_PZ_III_DROP": ("GA_AircraftDrop_GerSpotterPzIII", "Panzer IV Spotter and Panzer III Drop",
                                   "2 Ju 390s land a Panzer IV F1 Spotter and a Panzer III J at the target location.",
                                   (PZIV_SPOTTER, PZIII_J)),
}


def tank_entry(subtype, path):
    name = path.split("/")[-1]
    return '(TankSubtype=%s,TankAttachZOffset=-250.000000,TankClassOverride="%s%s.%s_C")' % (subtype, VEHICLES, path, name)


def fresh(template, prop):
    """A new struct per template: edited copies of get_editor_property have leaked between templates before."""
    old = template.get_editor_property(prop)
    new = type(old)()
    new.import_text(old.export_text())
    return new


def configure(ability, ability_key, source, object_values):
    _, title, description, vehicles = object_values
    ability.set_editor_property("M_AbilityType", ability_key)
    for prop in ("M_AbilityCosts", "M_AbilityRequirements", "M_AbilitySoundSettings", "M_AbilityMarker"):
        ability.set_editor_property(prop, fresh(source, prop))
    aim = fresh(source, "M_AimSettings")
    aim.import_text(AIM)
    ability.set_editor_property("M_AimSettings", aim)
    ui = fresh(source, "M_UISettings")
    ui.set_editor_property("Title", unreal.Text("<Text_Title>%s</>" % title))
    ui.set_editor_property("Description", unreal.Text(description))
    ui.set_editor_property("AbilityIcon", None)
    ability.set_editor_property("M_UISettings", ui)
    drop = fresh(source, "M_AircraftDropSettings")
    text = drop.export_text()
    text = re.sub(r'AircraftClass="[^"]*"', 'AircraftClass="%s"' % AIRCRAFT, text)
    tanks = ",".join(tank_entry(*vehicle) for vehicle in vehicles)
    text = re.sub(r"TanksWithOffsets=\(.*?\),SquadSubtypes", "TanksWithOffsets=(%s),SquadSubtypes" % tanks, text)
    drop.import_text(text)
    ability.set_editor_property("M_AircraftDropSettings", drop)


def setup():
    data_asset = unreal.load_asset(DATA_ASSET)
    templates = data_asset.get_editor_property("M_AbilityTemplatesByType")
    keys = list(templates.keys())
    enum = type(keys[0])
    by_name = {key.name: value for key, value in templates.items()}
    data_asset.modify()
    for value_name, values in DROPS.items():
        key = getattr(enum, value_name)
        ability = by_name.get(value_name) or unreal.new_object(unreal.GA_AircraftDrop, outer=data_asset, name=values[0])
        configure(ability, key, by_name[SOURCE_TEMPLATE], values)
        templates[key] = ability
    data_asset.set_editor_property("M_AbilityTemplatesByType", templates)
    print("SAVED_DA", unreal.EditorAssetLibrary.save_loaded_asset(data_asset, False))
    for value_name in DROPS:
        ability = data_asset.get_editor_property("M_AbilityTemplatesByType")[getattr(enum, value_name)]
        print("TEMPLATE", value_name, ability.get_path_name().split(":")[-1],
              ability.get_editor_property("M_AircraftDropSettings").export_text()[:60])


setup()
