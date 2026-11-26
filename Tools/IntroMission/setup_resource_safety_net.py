"""2026-11-26: resource safety net on the intro's base-building stage (re-runnable).

Mission_Intro_BaseBuilding gets PlayerResourceSafetyNet (UMissionBase) with the purchases the stage needs up to the
refinery's converter expansion, and a StartPlayerResourceSafetyNet call in BP_OnMissionStart right after
DestroyRadixiteGate. The subsystem waits for the HQ to stand and ends once all four are owned (or the stage ends).

Run headless so the UECP script bridge loads (do NOT disable BpGeneratorUltimate):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nullrhi -unattended -nosplash
  -DisablePlugins=Fab -NoLiveCoding
The map instance has never overridden the property, so it takes the class default; the map is not saved.
"""
import json

import unreal

BP_PATH = "/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/Mission_Intro_BaseBuilding"
OUT = unreal.Paths.project_saved_dir() + "IntroMission/resource_safety_net_setup.txt"
START_FUNCTION = "StartPlayerResourceSafetyNet"
GATE_CALL = "DestroyRadixiteGate"
LINES = []


def log(msg):
    LINES.append(str(msg))
    unreal.log("RES_SAFETY_NET " + str(msg))


def dispatch(category, action, args):
    return json.loads(unreal.UECPScriptBridge.dispatch(category, action, json.dumps(args)))


def enum_value(enum_cls, flat_name):
    """Numeric value of the member whose name without underscores is flat_name (FTrainingOption stores a uint8)."""
    for name in dir(enum_cls):
        if name.isupper() and name.replace("_", "") == flat_name:
            return int(getattr(enum_cls, name).value)
    raise RuntimeError("no %s member %s" % (enum_cls.__name__, flat_name))


def build_settings_text():
    harvester = enum_value(unreal.TankSubtype, "TANKPZIHARVESTER")
    gamma = enum_value(unreal.NomadicSubtype, "NOMADICGERGAMMAFACILITY")
    refinery = enum_value(unreal.NomadicSubtype, "NOMADICGERREFINERY")
    purchases = [
        "(bIsBuildingExpansion=False,Unit=(UnitType=UNType_Tank,SubtypeValue=%d))" % harvester,
        "(bIsBuildingExpansion=False,Unit=(UnitType=UNType_Nomadic,SubtypeValue=%d))" % gamma,
        "(bIsBuildingExpansion=False,Unit=(UnitType=UNType_Nomadic,SubtypeValue=%d))" % refinery,
        "(bIsBuildingExpansion=True,BuildingExpansion=BTX_RefConverter)",
    ]
    return "(RequiredPurchases=(%s))" % ",".join(purchases)


def apply_settings():
    generated = unreal.load_object(None, BP_PATH + ".Mission_Intro_BaseBuilding_C")
    cdo = unreal.get_default_object(generated)
    settings = cdo.get_editor_property("player_resource_safety_net")
    text = build_settings_text()
    settings.import_text(text)
    cdo.set_editor_property("player_resource_safety_net", settings)
    readback = cdo.get_editor_property("player_resource_safety_net").export_text()
    log("settings: " + readback)
    # Default (False) fields are not exported: count the three unit entries and the converter.
    if "BTX_RefConverter" not in readback or readback.count("SubtypeValue=") != 3:
        raise RuntimeError("settings did not apply: " + readback)


def find_node(snapshot, predicate):
    for node in snapshot.get("nodes", []):
        if predicate(node):
            return node
    return None


def node_calls(node, function_name):
    detail = node.get("detail") or {}
    return function_name in json.dumps(detail) or function_name in node.get("title", "")


def wire_start_call():
    snapshot = dispatch("blueprint", "get_graph_snapshot", {
        "blueprint_path": BP_PATH, "graph_name": "EventGraph", "view": "compact", "include_hashes": True})
    if find_node(snapshot, lambda node: node_calls(node, START_FUNCTION)):
        log("start call already in the event graph")
        return
    gate_call = find_node(snapshot, lambda node: node_calls(node, GATE_CALL))
    if gate_call is None:
        raise RuntimeError("DestroyRadixiteGate call not found in the event graph")
    result = dispatch("blueprint", "splice_graph", {
        "blueprint_path": BP_PATH, "graph_name": "EventGraph",
        "patch": [{"op": "insert_between", "from": {"node_id": gate_call["id"], "pin_name": "then"},
                   "handle": "fn.Self." + START_FUNCTION, "expected_hash": gate_call["content_hash"]}]})
    log("splice: " + json.dumps(result))
    if not result.get("applied"):
        raise RuntimeError("splice failed")


def main():
    blueprint = unreal.load_asset(BP_PATH)
    apply_settings()
    wire_start_call()
    compiled = dispatch("blueprint", "compile_blueprint", {"blueprint_path": BP_PATH})
    log("compile: " + json.dumps(compiled))
    if compiled.get("error_count", 1) != 0:
        raise RuntimeError("compile errors; not saved")
    intent = dispatch("blueprint", "get_graph_intent", {
        "blueprint_path": BP_PATH, "graph_name": "EventGraph", "root": "BP_OnMissionStart"})
    log("intent: " + intent.get("intent", ""))
    log("saved: %s" % unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False))


try:
    main()
except Exception as error:  # noqa: BLE001 - the log file is the only output of a headless run
    log("FAILED: %s" % error)
finally:
    unreal.Paths.make_platform_filename(OUT)
    import os
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as handle:
        handle.write("\n".join(LINES) + "\n")
