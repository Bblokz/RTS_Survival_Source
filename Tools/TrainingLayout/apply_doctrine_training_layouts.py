"""Writes the doctrine training grids into DA_TrainingLayout without touching any other building.

Run inside an editor whose C++ already has the subtypes used below, either through the UECP bridge
(execute_python with this file's contents) or headless:
    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>"

Every grid is three slots wide and filled row by row; "-" is an empty slot.
"""
import unreal

LAYOUT_ASSET_PATH = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DA_TrainingLayout.DA_TrainingLayout"
SLOTS_PER_SUB_SECTION = 9
EMPTY = "-"

# Keys are the C++ enum value names, so the script reads like the TrainingLayoutDataAsset defaults.
LAYOUTS = {
    "GerBreakthroughDoctrine": {
        "Nomadic_GerMechanizedDepot": [
            ("Recons", [
                "T:Tank_Sdkfz250", "T:Tank_Pz38t", "T:Tank_Sdkfz_221",
                EMPTY, "T:Tank_Sdkfz250_37mm", EMPTY,
            ]),
            ("Support Weapons", [
                EMPTY, "S:Squad_Ger_PaK38", EMPTY,
                "S:Squad_Ger_LefH18", "S:Squad_Ger_PaK40", EMPTY,
            ]),
        ],
        "Nomadic_GerLightSteelForge": [
            ("Light Tanks", [
                "T:Tank_PzII_Luchs", "T:Tank_Sdkfz_7_Flakvierling", "T:Tank_PzJager",
                "T:Tank_PzI_15cm", "T:Tank_PzIV_F1", "T:Tank_Pz38t_RailGun",
            ]),
        ],
        "Nomadic_GerMedTankFactory": [
            ("Medium Tanks", [
                "T:Tank_Marder", "T:Tank_PzIV_G", "T:Tank_PzIV_70",
                "T:Tank_Brumbar", "T:Tank_PzIV_H", EMPTY,
                EMPTY, "T:Tank_PZIV_Rockets", EMPTY,
            ]),
        ],
    },
    "GerStrikeDivision": {
        "Nomadic_GerMechanizedDepot": [
            ("Recons", [
                "T:Tank_Sdkfz251_Transport", "T:Tank_Sdkfz_140", "T:Tank_Sdkfz_221",
                "T:Tank_Sdkfz251_Mortar", "T:Tank_Sdkfz251_22", EMPTY,
            ]),
            ("Support Weapons", [
                EMPTY, "S:Squad_Ger_PaK38", EMPTY,
                "S:Squad_Ger_SFH18_150mm", "S:Squad_Ger_88mmFlak", EMPTY,
            ]),
        ],
        "Nomadic_GerLightSteelForge": [
            ("Light Tanks", [
                "T:Tank_PzII_F", "T:Tank_Puma", "T:Tank_PzJagerLaser",
                "T:Tank_Sdkfz_232_3", "T:Tank_PzIII_J", "T:Tank_PzIII_AA",
                EMPTY, "T:Tank_Pz38t_R", EMPTY,
            ]),
        ],
        "Nomadic_GerMedTankFactory": [
            ("Medium Tanks", [
                "T:Tank_Stug", "T:Tank_Jaguar", "T:Tank_Hetzer",
                "T:Tank_Wespe", "T:Tank_PzIII_M", "T:Tank_PzIII_FLamm",
            ]),
        ],
    },
}


def normalise(name):
    return name.replace("_", "").lower()


def resolve_enum(enum_class, cpp_name):
    """Python spells enum values in upper snake case with its own word splits; match ignoring underscores."""
    wanted = normalise(cpp_name)
    for attribute in dir(enum_class):
        if attribute.isupper() and normalise(attribute) == wanted:
            return getattr(enum_class, attribute)
    raise RuntimeError(f"{enum_class.__name__} has no value {cpp_name}; is the editor running the new C++?")


def make_slot(slot_code):
    layout_slot = unreal.TrainingLayoutSlot()
    if slot_code == EMPTY:
        return layout_slot
    kind, cpp_name = slot_code.split(":", 1)
    if kind == "T":
        layout_slot.set_editor_property("unit_type", unreal.AllUnitType.UN_TYPE_TANK)
        layout_slot.set_editor_property("tank_subtype", resolve_enum(unreal.TankSubtype, cpp_name))
    elif kind == "S":
        layout_slot.set_editor_property("unit_type", unreal.AllUnitType.UN_TYPE_SQUAD)
        layout_slot.set_editor_property("squad_subtype", resolve_enum(unreal.SquadSubtype, cpp_name))
    else:
        raise RuntimeError(f"Unknown slot kind in {slot_code}")
    return layout_slot


def make_sub_section(display_name, slot_codes, existing_sub_sections):
    padded_codes = slot_codes + [EMPTY] * (SLOTS_PER_SUB_SECTION - len(slot_codes))
    sub_section = unreal.TrainingLayoutSubSection()
    # Reuse the existing localised text of a sub section with this name, so its translation key survives.
    display_text = unreal.Text(display_name)
    for existing_sub_section in existing_sub_sections:
        existing_text = existing_sub_section.get_editor_property("display_name")
        if str(existing_text) == display_name:
            display_text = existing_text
            break
    sub_section.set_editor_property("display_name", display_text)
    sub_section.set_editor_property("slots", [make_slot(code) for code in padded_codes])
    return sub_section


def apply_layouts():
    layout_asset = unreal.load_asset(LAYOUT_ASSET_PATH)
    faction_layouts = layout_asset.get_editor_property("faction_layouts")
    for faction_name, buildings_to_set in LAYOUTS.items():
        faction = resolve_enum(unreal.RTSFaction, faction_name)
        faction_layout = faction_layouts[faction]
        buildings = faction_layout.get_editor_property("buildings")
        for building_name, sub_section_specs in buildings_to_set.items():
            building_key = resolve_enum(unreal.NomadicSubtype, building_name)
            building = buildings[building_key]
            existing_sub_sections = building.get_editor_property("sub_sections")
            building.set_editor_property("sub_sections", [
                make_sub_section(name, codes, existing_sub_sections) for name, codes in sub_section_specs])
            buildings[building_key] = building
            unreal.log(f"Training layout: set {faction_name} / {building_name}")
        faction_layout.set_editor_property("buildings", buildings)
        faction_layouts[faction] = faction_layout
    layout_asset.modify()
    layout_asset.set_editor_property("faction_layouts", faction_layouts)
    if not unreal.EditorAssetLibrary.save_loaded_asset(layout_asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save DA_TrainingLayout")
    unreal.log("Training layout: saved DA_TrainingLayout")


apply_layouts()
