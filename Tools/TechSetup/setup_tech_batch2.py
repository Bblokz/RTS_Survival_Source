"""Creates and wires the second batch of German technologies (Tungsten-Core 8.8 ... Entrenching Tools).

Steps (each idempotent; run again after a failure):
  icons     import the NoAIGen icons next to the existing tech icons
  assets    behaviour Blueprints (buff icon, texts, weapon multipliers) and TEBP effect Blueprints
  buttons   research rows (title/description/icon per tech) and the IdActivateMode/IdDisableMode rows for the
            Flak 88 ground role toggle in Buttons_DataTableb
  buffs     buff icon entries in DA_BehaviourUI
  research  research slots (ResearchTechnologyAbilityComp) on the host building expansions
  techmap   ETechnology -> TEBP entries in RTS_BP_PlayerController GetPlayerTechs (C++ console command)
Headless:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/TechSetup/setup_tech_batch2.py all"
Saving a package another editor has loaded fails; every save is checked and reported as TECH_SETUP_FAIL.
"""
import json

import sys

import unreal

ICON_SOURCE = "D:/Blender/BlenderProjects/BlenderTanks/AbilityIcons/Technologies/NoAIGen"
ICON_FOLDER = "/Game/RTS_Survival/Textures/GameUI/TechTree/GerTech"
ICON_SETTINGS_TEMPLATE = ICON_FOLDER + "/T_TechIcon_Spall_Liners"
TECH_FOLDER = "/Game/RTS_Survival/Blueprints/TechTree/GerTechs"
BUTTONS = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
BEHAVIOUR_UI = "/Game/RTS_Survival/Blueprints/GameUI/BehaviourUI/DA_BehaviourUI"
CONTROLLER = "/Game/RTS_Survival/Blueprints/Player/RTS_BP_PlayerController"
CONTROLLER_TECH_GRAPH = "GetPlayerTechs"
BXP_FOLDER = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps"
GERMAN_FACTIONS = ("GER_BREAKTHROUGH_DOCTRINE", "GER_STRIKE_DIVISION")

# key, ETechnology name, folder, icon texture, buff icon enum, buff title, buff text, research title, research text
TECHS = [
    dict(key="Tungsten88", tech="Tech_Tungsten88", icon="T_TechIcon_Tungsten88", buff_icon="TechTungsten88",
         title="Tungsten-Core 8.8",
         buff_text="Tungsten-core rounds for the long 8.8 cm gun: <Text_sExp>+15%</> armour penetration",
         research_text="Tungsten-core rounds for the long 8.8 cm guns of the Tiger, Tiger H1, King Tiger, Jagdpanther "
                       "and Panther II: <Text_sExp>+15%</> armour penetration at every range",
         weapon=dict(armor_penetration_mlt=1.15, min_calibre=88.0, max_calibre=88.0)),
    dict(key="StereoRangefinder", tech="Tech_StereoRangefinder", icon="T_TechIcon_StereoRangefinder",
         buff_icon="TechStereoRangefinder", title="Stereoscopic Rangefinder",
         buff_text="Main gun: <Text_sExp>+10%</> range and <Text_sExp>+15%</> accuracy (up to the 100 cap)",
         research_text="Fits the StuG, Hetzer, Panzer IV/70, Jagdpanther and Jagdtiger with a stereoscopic "
                       "rangefinder: <Text_sExp>+10%</> range and <Text_sExp>+15%</> accuracy (up to the 100 cap) for the main gun. "
                       "Stacks with Long-Range Calibration",
         weapon=dict(range_mlt=1.10, accuracy_mlt=1.15, min_calibre=37.0, max_calibre=0.0)),
    dict(key="ShellHoist", tech="Tech_Assault_ShellHoist", icon="T_TechIcon_ShellHoist", buff_icon="TechShellHoist",
         title="Heavy Shell Hoist", buff_text="Main gun reload time <Text_sExp>-30%</>",
         research_text="A powered shell hoist for the Brummbar, Sturmtiger and Panzer I 15cm: main gun reload time "
                       "<Text_sExp>-30%</>",
         weapon=dict(reload_speed_mlt=0.70, min_calibre=100.0, max_calibre=0.0)),
    dict(key="ArmoredTroopBay", tech="Tech_HalfTrack_ArmoredBay", icon="T_TechIcon_ArmoredTroopBay",
         buff_icon="TechArmoredTroopBay", title="Armoured Troop Bay",
         buff_text="<Text_sExp>+20%</> hull armour. Passengers bail out with half their health when the vehicle is "
                   "destroyed",
         research_text="All SdKfz 250 and 251 half-tracks gain <Text_sExp>+20%</> hull armour. \\nWhen a half-track is "
                       "destroyed its passengers bail out with half their health instead of dying with it"),
    dict(key="Flak88GroundRole", tech="Tech_Flak88_GroundRole", icon="T_TechIcon_Flak88GroundRole",
         buff_icon="TechFlak88GroundRole", title="Flak 88 Ground Role",
         buff_text="Can switch to the ground role: <Text_sExp>+20%</> armour penetration against tanks while active",
         research_text="Unlocks the ground role for the 8.8 cm Flak: while the mode is active the gun fires "
                       "armour-piercing rounds with <Text_sExp>+20%</> penetration against tanks"),
    dict(key="InfantryKit", tech="Tech_Infantry_Kit", icon="T_TechIcon_InfantryKit", buff_icon="TechInfantryKit",
         title="M42 Kit & Webbing", buff_text="<Text_sExp>+15%</> health",
         research_text="Jager, Panzergrenadiere, Gebirgsjager and LMG squads gain <Text_sExp>+15%</> health. Kept "
                       "when the squad reinforces; stacks with Ghillie Suits"),
    dict(key="GhillieSuits", tech="Tech_Sniper_Ghillie", icon="T_TechIcon_GhillieSuits", buff_icon="TechGhillieSuits",
         title="Ghillie Suits", buff_text="<Text_sExp>+33%</> health",
         research_text="Sniper teams and Gebirgsjager gain <Text_sExp>+33%</> health. Gebirgsjager with M42 Kit get "
                       "both bonuses (<Text_sExp>+53%</>)"),
    dict(key="EntrenchingTools", tech="Tech_Pioneer_Entrenching", icon="T_TechIcon_EntrenchingTools",
         buff_icon="TechEntrenchingTools", title="Entrenching Tools",
         buff_text="Builds field defences <Text_sExp>35%</> faster",
         research_text="Sturmpioniere, scavengers and medic scavengers build field defences <Text_sExp>35%</> faster"),
]
FLAK_MODE = dict(name="BP_Beh_Flak88GroundRoleMode", icon="T_TechIcon_Flak88GroundRole_Mode",
                 title="Ground Role", text="Armour-piercing ground fire: <Text_sExp>+20%</> armour penetration",
                 weapon=dict(armor_penetration_mlt=1.20, min_calibre=88.0, max_calibre=88.0),
                 mode_type_value=2, activate_title="Ground Role",
                 activate_text="Switch the 8.8 to armour-piercing ground fire: <Text_sExp>+20%</> armour penetration "
                               "against tanks",
                 disable_title="Leave Ground Role", disable_text="Return the 8.8 to its normal fire")

# Host building expansion -> list of (component name, preferred index, [tech keys in chain order], costs)
RESEARCH_HOSTS = {
    "Barracks/RadixReactor/Ger_Bxp_BarracksReactorExpansion": [
        ("InfantryKitTech", 10, ["InfantryKit"], {"RESOURCE_RADIXITE": 400, "RESOURCE_METAL": 150}),
        ("GhillieSuitsTech", 11, ["GhillieSuits"], {"RESOURCE_RADIXITE": 350, "RESOURCE_METAL": 100}),
        ("EntrenchingToolsTech", 12, ["EntrenchingTools"], {"RESOURCE_RADIXITE": 300, "RESOURCE_METAL": 100}),
    ],
    "Barracks/FuelCell/Ger_Bxp_BarracksFuelExpansion": [
        ("Flak88GroundRoleTech", 10, ["Flak88GroundRole"], {"RESOURCE_RADIXITE": 600, "RESOURCE_VEHICLE_PARTS": 100}),
    ],
    "Forge/RadixFuel/Ger_Bxp_LFactoryFuelExpansion": [
        ("ArmoredTroopBayTech", 7, ["ArmoredTroopBay"], {"RESOURCE_RADIXITE": 500, "RESOURCE_VEHICLE_PARTS": 150}),
    ],
    "Forge/RadixReactor/Ger_Bxp_LFactoryRadixiteReactor": [
        # The rangefinder comes first; Tungsten-Core 8.8 follows in the same slot once it is researched.
        ("RangefinderTungstenTech", 13, ["StereoRangefinder", "Tungsten88"],
         {"RESOURCE_RADIXITE": 600, "RESOURCE_VEHICLE_PARTS": 150}),
        ("ShellHoistTech", 14, ["ShellHoist"], {"RESOURCE_RADIXITE": 700, "RESOURCE_VEHICLE_PARTS": 200}),
    ],
}
FOLLOW_UP_COSTS = {"Tungsten88": {"RESOURCE_RADIXITE": 1000, "RESOURCE_VEHICLE_PARTS": 350}}

FAILURES = []


def log(message):
    unreal.log("TECH_SETUP " + message)


def fail(message):
    FAILURES.append(message)
    unreal.log_error("TECH_SETUP_FAIL " + message)


def enum_value(enum_class, cpp_name):
    wanted = cpp_name.replace("_", "").lower()
    for attribute in dir(enum_class):
        if attribute.isupper() and attribute.replace("_", "").lower() == wanted:
            return getattr(enum_class, attribute)
    raise RuntimeError(f"{enum_class.__name__} has no {cpp_name}; is this editor running the new C++?")


def tech_index(cpp_name):
    return int(enum_value(unreal.Technology, cpp_name).value)


def save(asset_or_package_path, label):
    path = asset_or_package_path if isinstance(asset_or_package_path, str) else asset_or_package_path.get_path_name()
    package = path.split(".")[0]
    if not unreal.EditorAssetLibrary.save_asset(package, only_if_is_dirty=False):
        fail(f"save {label} ({package}) - another editor probably has it loaded")
        return False
    log(f"saved {label}")
    return True


def tech_class_path(entry, prefix):
    name = f"{prefix}{entry['key']}"
    return f"{TECH_FOLDER}/{entry['key']}/{name}"


# ---------------------------------------------------------------------------------------------------------- icons

def step_icons():
    template = unreal.load_asset(ICON_SETTINGS_TEMPLATE)
    names = [entry["icon"] for entry in TECHS] + [FLAK_MODE["icon"]]
    tasks = []
    for name in names:
        task = unreal.AssetImportTask()
        task.filename = f"{ICON_SOURCE}/{name}.png"
        task.destination_path = ICON_FOLDER
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name in names:
        texture = unreal.load_asset(f"{ICON_FOLDER}/{name}")
        if texture is None:
            fail(f"icon import {name}")
            continue
        for prop in ("compression_settings", "lod_group", "srgb", "mip_gen_settings", "never_stream"):
            texture.set_editor_property(prop, template.get_editor_property(prop))
        save(texture, name)


# --------------------------------------------------------------------------------------------------------- assets

def create_blueprint(folder, name, parent_class):
    path = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        blueprint = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    return blueprint


def default_object(blueprint):
    return unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(blueprint))


def configure_behaviour(cdo, buff_icon, title, text, weapon=None):
    cdo.set_editor_property("BehaviourIcon", enum_value(unreal.BehaviourIcon, buff_icon))
    cdo.set_editor_property("M_TitleText", f"<Text_Title>{title}</>")
    cdo.set_editor_property("M_DisplayText", text)
    cdo.set_editor_property("M_BuffType", unreal.BuffDebuffType.BUFF)
    cdo.set_editor_property("BehaviourLifeTime", unreal.BehaviourLifeTime.PERMANENT)
    cdo.set_editor_property("BehaviourStackRule", unreal.BehaviourStackRule.NONE)
    cdo.set_editor_property("M_MaxStackCount", 1)
    if not weapon:
        return
    multipliers = cdo.get_editor_property("BehaviourWeaponMultipliers")
    for field in ("armor_penetration_mlt", "range_mlt", "accuracy_mlt", "reload_speed_mlt"):
        if field in weapon:
            multipliers.set_editor_property(field, weapon[field])
    cdo.set_editor_property("BehaviourWeaponMultipliers", multipliers)
    cdo.set_editor_property("MinimalWeaponCalibreToApplyTo", weapon["min_calibre"])
    cdo.set_editor_property("MaximumWeaponCalibreToApplyTo", weapon["max_calibre"])


def step_assets():
    for entry in TECHS:
        folder = f"{TECH_FOLDER}/{entry['key']}"
        parent = unreal.BehaviourWeapon if entry.get("weapon") else unreal.Behaviour
        behaviour = create_blueprint(folder, f"BP_Beh_Tech_{entry['key']}", parent)
        configure_behaviour(default_object(behaviour), entry["buff_icon"], entry["title"], entry["buff_text"],
                            entry.get("weapon"))
        unreal.BlueprintEditorLibrary.compile_blueprint(behaviour)
        save(behaviour, behaviour.get_name())

        effect_class = getattr(unreal, "TE_" + entry["key"])
        effect = create_blueprint(folder, f"TEBP_Ger_{entry['key']}", effect_class)
        effect_cdo = default_object(effect)
        effect_cdo.set_editor_property("BehaviourClassToApply",
                                       unreal.BlueprintEditorLibrary.generated_class(behaviour))
        if entry["key"] == "Flak88GroundRole":
            mode = create_blueprint(folder, FLAK_MODE["name"], unreal.BehaviourWeapon)
            configure_behaviour(default_object(mode), entry["buff_icon"], FLAK_MODE["title"], FLAK_MODE["text"],
                                FLAK_MODE["weapon"])
            unreal.BlueprintEditorLibrary.compile_blueprint(mode)
            save(mode, mode.get_name())
            effect_cdo.set_editor_property("GroundRoleModeBehaviour", unreal.BlueprintEditorLibrary.generated_class(mode))
        unreal.BlueprintEditorLibrary.compile_blueprint(effect)
        save(effect, effect.get_name())
        if default_object(effect).get_editor_property("Technology") != enum_value(unreal.Technology, entry["tech"]):
            fail(f"{effect.get_name()} technology does not match {entry['tech']}")


# -------------------------------------------------------------------------------------------------------- buttons

def text_value(text):
    return '"' + text.replace('"', "'") + '"'


def custom_entry(title, description, icon_name):
    icon = f"/Script/Engine.Texture2D'{ICON_FOLDER}/{icon_name}.{icon_name}'"
    return (f"(CustomTitle={text_value(title)},CustomDescription={text_value(description)},"
            f"CustomIcon=\"{icon}\")")


def step_buttons():
    table = unreal.load_asset(BUTTONS)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    by_name = {row["Name"]: row for row in rows}
    research = by_name["IdResearchTechnology"]
    for entry in TECHS:
        research["CustomTypeMapping"][str(tech_index(entry["tech"]))] = custom_entry(
            f"<Text_Title>Research {entry['title']}</>", entry["research_text"], entry["icon"])
    mode_key = str(FLAK_MODE["mode_type_value"])
    for row_name, title, text in (("IdActivateMode", FLAK_MODE["activate_title"], FLAK_MODE["activate_text"]),
                                  ("IdDisableMode", FLAK_MODE["disable_title"], FLAK_MODE["disable_text"])):
        if row_name not in by_name:
            row = {"Name": row_name, "Icon": research["Icon"], "Description": "Mode", "Title": "Mode",
                   "CustomTypeMapping": {}}
            rows.append(row)
            by_name[row_name] = row
        by_name[row_name]["CustomTypeMapping"][mode_key] = custom_entry(
            f"<Text_Title>{title}</>", text, FLAK_MODE["icon"])
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
        fail("Buttons_DataTableb import")
        return
    check = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    if len(check) != len(rows):
        fail(f"Buttons_DataTableb row count {len(check)} != {len(rows)}")
        return
    save(table, "Buttons_DataTableb")


# ---------------------------------------------------------------------------------------------------------- buffs

def step_buffs():
    asset = unreal.load_asset(BEHAVIOUR_UI)
    styles = asset.get_editor_property("BehaviourIconStyles")
    for entry in TECHS:
        style = unreal.BehaviourIconWidgetStyle()
        style.set_editor_property("IconTexture", unreal.load_asset(f"{ICON_FOLDER}/{entry['icon']}"))
        styles[enum_value(unreal.BehaviourIcon, entry["buff_icon"])] = style
    asset.set_editor_property("BehaviourIconStyles", styles)
    save(asset, "DA_BehaviourUI")


# ------------------------------------------------------------------------------------------------------- research

def make_unit_cost(costs):
    # ResourceCosts is EditDefaultsOnly, so Python cannot set it on an instance; text import can.
    pairs = []
    for resource, amount in costs.items():
        enum_name = str(enum_value(unreal.RTSResourceType, resource)).split(".")[-1].split(":")[0]
        cpp_name = next(name for name in ("Resource_Radixite", "Resource_Metal", "Resource_VehicleParts")
                        if name.replace("_", "").lower() == enum_name.replace("_", "").lower())
        pairs.append(f"({cpp_name}, {amount})")
    unit_cost = unreal.UnitCost()
    unit_cost.import_text("(ResourceCosts=(" + ",".join(pairs) + "))")
    return unit_cost


def make_technology_data(outer, keys, costs):
    by_key = {entry["key"]: entry for entry in TECHS}
    data = unreal.new_object(unreal.ResearchTechnologyAbilityTechnologyData, outer=outer)
    data.set_editor_property("technology", enum_value(unreal.Technology, by_key[keys[0]]["tech"]))
    data.set_editor_property("costs", make_unit_cost(costs))
    if len(keys) > 1:
        follow_up = make_technology_data(outer, keys[1:], FOLLOW_UP_COSTS[keys[1]])
        data.set_editor_property("follow_up_technologies", [follow_up])
    return data


def find_component_template(blueprint, component_name):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if template and template.get_name().startswith(component_name):
            return template
    return None


def add_research_component(blueprint, component_name):
    template = find_component_template(blueprint, component_name)
    if template:
        return template
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)[0]
    params = unreal.AddNewSubobjectParams()
    params.set_editor_property("parent_handle", root)
    params.set_editor_property("new_class", unreal.ResearchTechnologyAbilityComp)
    params.set_editor_property("blueprint_context", blueprint)
    handle, reason = subsystem.add_new_subobject(params)
    if not unreal.SubobjectDataBlueprintFunctionLibrary.is_handle_valid(handle):
        fail(f"add component to {blueprint.get_name()}: {reason}")
        return None
    subsystem.rename_subobject(handle, unreal.Text(component_name))
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    return unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)


def step_research():
    for host, slots in RESEARCH_HOSTS.items():
        blueprint = unreal.load_asset(f"{BXP_FOLDER}/{host}")
        for component_name, index, keys, costs in slots:
            template = add_research_component(blueprint, component_name)
            if template is None:
                continue
            settings_map = template.get_editor_property("TechAbilitySettingsPerFaction")
            for faction in GERMAN_FACTIONS:
                settings = unreal.ResearchTechnologyAbilitySettings()
                settings.set_editor_property("PreferredAbilityIndex", index)
                settings.set_editor_property("TechnologyData", make_technology_data(template, keys, costs))
                settings_map[getattr(unreal.RTSFaction, faction)] = settings
            template.set_editor_property("TechAbilitySettingsPerFaction", settings_map)
            log(f"{blueprint.get_name()} {component_name} -> {keys} index {index}")
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint, blueprint.get_name())


# -------------------------------------------------------------------------------------------------------- techmap

def step_techmap():
    pairs = []
    for entry in TECHS:
        effect = tech_class_path(entry, "TEBP_Ger_")
        pairs.append(f"{entry['tech']}={effect}.TEBP_Ger_{entry['key']}_C")
    command = f"RTS.TechSetup.AddMakeMapEntries {CONTROLLER} {CONTROLLER_TECH_GRAPH} " + " ".join(pairs)
    unreal.SystemLibrary.execute_console_command(None, command)
    log("techmap command sent; see RTS_TECH_SETUP RESULT in the log")


STEPS = {"icons": step_icons, "assets": step_assets, "buttons": step_buttons, "buffs": step_buffs,
         "research": step_research, "techmap": step_techmap}
ORDER = ["icons", "assets", "buttons", "buffs", "research", "techmap"]


def main(arguments):
    selected = ORDER if not arguments or arguments[0] == "all" else arguments
    for step in selected:
        log(f"step {step}")
        try:
            STEPS[step]()
        except Exception as error:  # noqa: BLE001 - one failing step must not hide the others
            fail(f"step {step} raised {error!r}")
    log("RESULT " + ("PASS" if not FAILURES else "FAIL " + " | ".join(FAILURES)))


if __name__ == "__main__":
    main(sys.argv[1:])
