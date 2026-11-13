"""Throwaway-editor check of the converter card icons (never the shared editor):
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=<map with a player converter>
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
(run_card_test.ps1 does that.)
The map needs a player converter that is BUILT on a deployed refinery. Ger_DestroyT35 (2026-11-12) only has a
level-placed Ger_Ref_Converter without an owner: it never reaches BXS_Built, so its card is empty by design. A player
T-34 is selected first as a control, so an empty converter card is not mistaken for a UI that is not live yet.

In PIE: finds a player-owned Ger_Ref_Converter, selects it through ACPPController::SelectSingleUnitFromUI, and reads
the command card buttons (BP_W_ActionUI_Item: ability, CustomType, image texture). Then clicks the conversion
button the way the player does (OnActionUIClicked) and reads the card again, so both the Enable and the Disable
icon are seen. Writes Saved/ResourceConversionIcons/card_test_result.json and quits.
"""
import json

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/ResourceConversionIcons/card_test_result.json"
CONVERTER_CLASS = "Ger_Ref_Converter_C"
ITEM_CLASS = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/BP_W_ActionUI_Item.BP_W_ActionUI_Item_C"
PIE_SETTLE_SECONDS = 45.0
S = {"phase": "wait_load", "t": 0.0, "mark": 0.0, "log": [], "reads": []}


def log(message):
    S["log"].append(message)
    unreal.log("CONVCARD " + message)


def finish():
    json.dump({"log": S["log"], "reads": S["reads"]}, open(OUT, "w"), indent=1)
    unreal.SystemLibrary.quit_editor()
    S["phase"] = "done"


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def converters(world):
    found = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if actor.get_class().get_name() != CONVERTER_CLASS:
            continue
        rts = actor.get_component_by_class(unreal.RTSComponent)
        found.append((actor, rts.get_editor_property("owning_player") if rts else None))
    return found


def card_items():
    item_class = unreal.load_class(None, ITEM_CLASS)
    items = []
    for widget in unreal.ObjectIterator(item_class):
        # Skip the class default object and widgets of the editor designer; PIE widgets live under the game instance.
        if widget.get_name().startswith("Default__") or "WidgetTree" in widget.get_path_name() and "GameInstance" not in widget.get_path_name():
            continue
        items.append(widget)
    return items


def dump_items(label):
    items = card_items()
    log("%s: %d items, e.g. %s" % (label, len(items), [i.get_path_name() for i in items[:3]]))
    for item in items:
        log("  %s ability=%s type=%s vis=%s" % (item.get_name(), item.get_editor_property("LastLoadedAbility"),
                                               item.get_editor_property("LastLoadedCustomType"), item.get_visibility()))


def read_card(label):
    entries = []
    for item in card_items():
        ability = item.get_editor_property("LastLoadedAbility")
        if "NO_ABILITY" in str(ability):
            continue
        image = item.get_editor_property("M_ActionItemImage")
        resource = image.get_editor_property("brush").get_editor_property("resource_object") if image else None
        entries.append({"ability": str(ability), "custom_type": item.get_editor_property("LastLoadedCustomType"),
                        "icon": resource.get_name() if resource else None,
                        "visible": str(item.get_visibility()), "widget": item.get_name()})
    S["reads"].append({"label": label, "entries": entries})
    log("%s: %s" % (label, [(e["ability"], e["custom_type"], e["icon"]) for e in entries]))
    return [item for item in card_items()
            if "RESOURCE_CONVERSION" in str(item.get_editor_property("LastLoadedAbility"))]


# (ability, CustomType) -> texture the card must show: the converter subtype and the rows' own icon for subtype 0.
ICON_LOOKUP_CASES = [
    ("ID_ENABLE_RESOURCE_CONVERSION", 1, "T_AB_ConvertRadixiteToVehicleParts"),
    ("ID_DISABLE_RESOURCE_CONVERSION", 1, "T_AB_CancelConvertRadixiteToVehicleParts"),
    ("ID_ENABLE_RESOURCE_CONVERSION", 0, "T_ActionUI_Shield"),
    ("ID_DISABLE_RESOURCE_CONVERSION", 0, "T_ActionUI_CancelJagdpantherRockets"),
]


def check_icon_lookup():
    """Feeds the live card button the converter entries the C++ hands it and reads the brush it picks from
    Buttons_DataTableb, so the row lookup, the CustomTypeMapping and the saved table are all exercised."""
    item = card_items()[-1]
    for ability_name, custom_type, expected in ICON_LOOKUP_CASES:
        ability = getattr(unreal.AbilityID, ability_name)
        item.call_method("OnUpdateActionUI", (ability, custom_type, 0, 0))
        image = item.get_editor_property("M_ActionItemImage")
        resource = image.get_editor_property("brush").get_editor_property("resource_object")
        name = resource.get_name() if resource else None
        S["reads"].append({"label": "lookup", "ability": ability_name, "custom_type": custom_type, "icon": name,
                           "expected": expected, "ok": name == expected})
        log("lookup %s type %d -> %s (%s)" % (ability_name, custom_type, name, "OK" if name == expected else "FAIL"))


def tick(delta):
    S["t"] += delta
    phase = S["phase"]
    try:
        if phase == "wait_load" and S["t"] > 15.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S["phase"], S["mark"] = "wait_pie", S["t"]
        elif phase == "wait_pie":
            world = game_world()
            if world is not None and S["t"] - S["mark"] > PIE_SETTLE_SECONDS:
                # Control: a player tank's card, so an empty converter card is not just a UI that is not live yet.
                tanks = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TankMaster)
                         if a.get_component_by_class(unreal.RTSComponent).get_editor_property("owning_player") == 1]
                log("player tanks %s" % [t.get_name() for t in tanks[:3]])
                if tanks:
                    unreal.GameplayStatics.get_player_controller(world, 0).select_single_unit_from_ui(tanks[0])
                S["phase"], S["mark"] = "read_control", S["t"]
            elif S["t"] - S["mark"] > 240.0:
                log("pie never started")
                finish()
        elif phase == "read_control" and S["t"] - S["mark"] > 2.0:
            read_card("control tank")
            check_icon_lookup()
            world = game_world()
            found = converters(world)
            log("converters %s" % [(a.get_name(), p, str(a.get_actor_location()), a.is_hidden_ed() if hasattr(a, "is_hidden_ed") else "",
                                    [c.get_name() for c in a.get_components_by_class(unreal.ResourceConverterComponent)])
                                   for a, p in found])
            mine = [a for a, p in found if p == 1]
            if not mine:
                log("no player converter on this map")
                finish()
                return
            unreal.GameplayStatics.get_player_controller(world, 0).select_single_unit_from_ui(mine[0])
            S["phase"], S["mark"] = "read_first", S["t"]
        elif phase == "read_first" and S["t"] - S["mark"] > 2.0:
            dump_items("after select")
            buttons = read_card("selected")
            if not buttons:
                log("no conversion button on the card")
                finish()
                return
            buttons[0].on_action_ui_clicked()
            S["phase"], S["mark"] = "read_second", S["t"]
        elif phase == "read_second" and S["t"] - S["mark"] > 2.0:
            buttons = read_card("after first click")
            if buttons:
                buttons[0].on_action_ui_clicked()
            S["phase"], S["mark"] = "read_third", S["t"]
        elif phase == "read_third" and S["t"] - S["mark"] > 2.0:
            read_card("after second click")
            finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log("ERROR %s: %r" % (phase, error))
        finish()


unreal.register_slate_post_tick_callback(tick)
log("script loaded")
