"""Field workshop drop (Sky-Drop Quartermaster signature, Tier III): builds the workshop Blueprint and authors
GA_GerFieldWorkshopDrop in DA_GlobalAbilitySystem.

Run headless on the DebugGame build (the Development DLL lacks the FieldWorkshop classes and UGA_CompositeDrop):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Hold Saved/AgentLocks/debuggame_build.lockdir and da_globalabilities.lockdir while it runs. The shared editor must not
save BXP_FieldWorkshop or the data asset afterwards (its DLL would drop the new components / templates).

1. BXP_FieldWorkshop (Blueprints/GlobalAbilitySystem/FieldWorkshop/) = a copy of the HQ repair bay
   (Ger_Bxp_HQRepairBay: repair pad + automatic welder crane, already a building that repairs vehicles) with
   - its URadiusRepairAura set to UFieldWorkshopRepairBehaviour (25 HP/s, refreshed every 1 s) in RADIUS_CM;
   - a UFieldWorkshopRearmAura "RearmAura" carrying the Sd.Kfz. 250 ammo carrier's aura settings (reload buff
     BP_Beh_Weapon_AmmoResupply, rising AmmoResupply icon, ammo-belt ground shader), in RADIUS_CM, magazine top-up
     every TOP_UP_SECONDS;
   - a UFieldWorkshopComponent "FieldWorkshop": auras off while falling, WORKING_SECONDS of work after landing with
     the radius drawn, then the game's pack-up animation and removal.
   It keeps the bay's expansion type (BTX_GerHQRepairBay: its health, armour, vision), so no enum or data row changes.
2. GA_GerFieldWorkshopDrop = UGA_CompositeDrop:
   child 0 = GA_GerFieldBunkerDrop's building drop (Ju 390 at >= 60 m, giant canopy cluster) carrying the workshop;
   child 1 = GA_GerFlak88Drop's gun paradrop with two Squad_Ger_PaK40 (as Child_Pak40Paradrop of
             GA_GerEliteScavengersPakDrop), GUN_OFFSET to the side so the guns land beside the workshop, 3 s later.

Idempotent: reuses the Blueprint, its components and the template objects; touches only GA_GerFieldWorkshopDrop in
the data asset; structs are copied through export_text/import_text (a struct read from a template can alias it).
Checks every source template is unchanged after saving. Retries the save on error 32 (a -game run holds the DA).
"""
import time

import unreal

LOG_PREFIX = "[FieldWorkshop] "
DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
ICON_DIR = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/"
FOLDER = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/FieldWorkshop"
WORKSHOP = FOLDER + "/BXP_FieldWorkshop"
SOURCE_BAY = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/HQ/RepairHub/Ger_Bxp_HQRepairBay"
AMMO_CARRIER = ("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz-250/"
                "Ger_ArmoredCar_Sdkfz250_AmmoCarrier")

KEY = "GA_GerFieldWorkshopDrop"
TITLE = "Field Workshop Drop"
DESCRIPTION = ("A Ju 390 drops a field repair depot under a cluster of giant canopies. For 90 s it repairs your "
               "vehicles within 20 m (25 HP/s), makes them reload faster and tops up their magazines every 5 s; "
               "then the crew packs it up. A second transport parachutes two Pak 40 guns and their crews beside it "
               "to defend it.")
TIER = "TIER_III"
# Repair bay (300 R + 150 M) + 2x Pak 40 (450 R + 180 M each): drop band 125% of the radixite part, radixite only
# (user decision 2026-11-05): (300 + 900) x 1.25 = 1500.
COST_RADIXITE = 1500
COOLDOWN = 240
ICON = "T_GlobalAbility_GerParatroopers"  # placeholder

RADIUS_CM = 2000.0
AURA_INTERVAL_SECONDS = 1.0
TOP_UP_SECONDS = 5.0
WORKING_SECONDS = 90.0
PACK_UP_SECONDS = 6.0
WORKSHOP_CANOPY_SCALE = 3.0
WORKSHOP_CANOPY_COUNT = 3
GUN_OFFSET = (0.0, 3000.0)
GUN_DELAY = 3.0
TWO_GUN_RING_RADIUS = 650.0

BUILDING_SOURCE = "GA_GerFieldBunkerDrop"
GUN_SOURCE = "GA_GerFlak88Drop"
SOURCES = (BUILDING_SOURCE, GUN_SOURCE)
SETTINGS_PROPERTY = {
    "GA_Paratroopers": "m_paratroopers_settings",
    "GA_BuildingParachuteDrop": "m_building_drop_settings",
}
BASE_PROPERTIES = ("m_ability_type", "m_aim_settings")
SOURCE_PROPERTIES = BASE_PROPERTIES + ("m_ability_marker", "m_ability_sound_settings", "m_ability_costs",
                                       "m_ui_settings", "m_ability_tier")
SAVE_RETRIES = 12
SAVE_RETRY_SECONDS = 10.0

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
BFL = unreal.SubobjectDataBlueprintFunctionLibrary


def log(line):
    unreal.log(LOG_PREFIX + line)


def pythonize(ue_name):
    out = []
    for i, ch in enumerate(ue_name):
        prev = ue_name[i - 1] if i else ""
        nxt = ue_name[i + 1] if i + 1 < len(ue_name) else ""
        if ch.isupper() and prev and prev != "_" and (prev.islower() or (nxt.islower() and nxt != "")):
            out.append("_")
        out.append(ch)
    return "".join(out).upper()


def member(enum_cls, ue_name):
    for candidate in (pythonize(ue_name), ue_name.upper(), pythonize(ue_name).replace("__", "_")):
        value = getattr(enum_cls, candidate, None)
        if value is not None:
            return value
    raise KeyError("%s has no member %s" % (enum_cls.__name__, ue_name))


def fresh_copy(value):
    if not hasattr(value, "export_text") or not hasattr(value, "import_text"):
        return value
    copy = type(value)()
    copy.import_text(value.export_text())
    return copy


def save_with_retry(asset, label):
    for attempt in range(SAVE_RETRIES):
        if eal.save_loaded_asset(asset, False):
            return True
        log("save of %s failed (attempt %d), retrying" % (label, attempt + 1))
        time.sleep(SAVE_RETRY_SECONDS)
    return False


# ------------------------------------------------------------------ Blueprint

def blueprint_components(bp):
    result = []
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = BFL.get_data(handle)
        obj = BFL.get_object_for_blueprint(data, bp)
        result.append((handle, obj))
    return result


def find_component(bp, cls, exact=True):
    for _handle, obj in blueprint_components(bp):
        if obj is None:
            continue
        if (obj.get_class() == cls.static_class()) if exact else isinstance(obj, cls):
            return obj
    return None


def add_component(bp, cls, name):
    existing = find_component(bp, cls)
    if existing is not None:
        return existing
    root = sds.k2_gather_subobject_data_for_blueprint(bp)[0]
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=root, new_class=cls,
                                                                      blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError("adding %s failed: %s" % (name, fail))
    sds.rename_subobject(handle, unreal.Text(name))
    obj = BFL.get_object_for_blueprint(BFL.get_data(handle), bp)
    if obj is None:
        raise RuntimeError("no object for new component " + name)
    return obj


def ammo_carrier_aura_settings():
    carrier = unreal.load_asset(AMMO_CARRIER)
    aura = find_component(carrier, unreal.RadiusAOEBehaviourComponent, exact=True)
    if aura is None:
        raise RuntimeError("no ammo resupply aura on " + AMMO_CARRIER)
    return (fresh_copy(aura.get_editor_property("AOEBehaviourSettings")),
            fresh_copy(aura.get_editor_property("RadiusSettings")))


def set_repair_aura(bp):
    aura = find_component(bp, unreal.RadiusRepairAura)
    if aura is None:
        raise RuntimeError("no RadiusRepairAura on the workshop copy")
    settings = fresh_copy(aura.get_editor_property("AOEBehaviourSettings"))
    settings.set_editor_property("behaviour_types", [unreal.FieldWorkshopRepairBehaviour.static_class()])
    settings.set_editor_property("radius", RADIUS_CM)
    settings.set_editor_property("interval_seconds", AURA_INTERVAL_SECONDS)
    settings.set_editor_property("apply_strategy", member(unreal.InAOEBehaviourApplyStrategy, "ApplyEveryTick"))
    aura.set_editor_property("AOEBehaviourSettings", settings)
    radius = fresh_copy(aura.get_editor_property("RadiusSettings"))
    # The workshop draws its own disc for its working time; hover/selection would stack another on it.
    radius.set_editor_property("show_radius_type", member(unreal.AOeBehaviourShowRadiusType, "None"))
    radius.set_editor_property("host_behaviour_title_text", "<Text_Title>Field Workshop Repairs</>")
    aura.set_editor_property("RadiusSettings", radius)
    return aura


def set_rearm_aura(bp):
    aura = add_component(bp, unreal.FieldWorkshopRearmAura, "RearmAura")
    settings, radius = ammo_carrier_aura_settings()
    settings.set_editor_property("radius", RADIUS_CM)
    settings.set_editor_property("interval_seconds", AURA_INTERVAL_SECONDS)
    settings.set_editor_property("apply_strategy", member(unreal.InAOEBehaviourApplyStrategy, "ApplyOnlyOnEnter"))
    settings.set_editor_property("affect_squad_controllers", False)
    aura.set_editor_property("AOEBehaviourSettings", settings)
    radius.set_editor_property("show_radius_type", member(unreal.AOeBehaviourShowRadiusType, "None"))
    radius.set_editor_property("radius_type", member(unreal.RTSRadiusType, "FullCircle_AmmoResupplyAura"))
    radius.set_editor_property("host_behaviour_title_text", "<Text_Title>Field Workshop Rearms</>")
    aura.set_editor_property("RadiusSettings", radius)
    aura.set_editor_property("TopUpIntervalSeconds", TOP_UP_SECONDS)
    return aura


def set_workshop_component(bp):
    component = add_component(bp, unreal.FieldWorkshopComponent, "FieldWorkshop")
    settings = fresh_copy(component.get_editor_property("M_WorkshopSettings"))
    settings.set_editor_property("working_seconds", WORKING_SECONDS)
    settings.set_editor_property("pack_up_seconds", PACK_UP_SECONDS)
    settings.set_editor_property("working_radius_type", member(unreal.RTSRadiusType, "FullCircle_AmmoResupplyAura"))
    component.set_editor_property("M_WorkshopSettings", settings)
    return component


def build_workshop(report):
    if not eal.does_asset_exist(WORKSHOP):
        report.append("duplicated %s -> %s: %s" % (SOURCE_BAY, WORKSHOP, eal.duplicate_asset(SOURCE_BAY, WORKSHOP)))
    bp = unreal.load_asset(WORKSHOP)
    if bp is None:
        raise RuntimeError("workshop Blueprint missing after duplicate")
    bp.modify()
    repair = set_repair_aura(bp)
    rearm = set_rearm_aura(bp)
    workshop = set_workshop_component(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not save_with_retry(bp, "BXP_FieldWorkshop"):
        raise RuntimeError("BXP_FieldWorkshop not saved")
    disk = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + WORKSHOP.replace("/Game/", "") + ".uasset")
    import os
    if not os.path.exists(disk):
        raise RuntimeError("BXP_FieldWorkshop saved but not on disk: " + disk)
    report.append("workshop saved on disk: %s" % disk)
    report.append("repair aura: %s" % repair.get_editor_property("AOEBehaviourSettings").export_text()[:500])
    report.append("rearm aura: %s" % rearm.get_editor_property("AOEBehaviourSettings").export_text()[:700])
    report.append("rearm radius: %s" % rearm.get_editor_property("RadiusSettings").export_text())
    report.append("workshop: %s" % workshop.get_editor_property("M_WorkshopSettings").export_text())
    return bp


# ------------------------------------------------------------------ template

def find_child(parent, name):
    return unreal.find_object(None, "%s.%s" % (parent.get_path_name(), name))


def source_snapshot(templates, ga_enum):
    snapshot = {}
    for key_name in SOURCES:
        source = templates[member(ga_enum, key_name)]
        properties = SOURCE_PROPERTIES + (SETTINGS_PROPERTY[source.get_class().get_name()],)
        snapshot[key_name] = "|".join(
            str(source.get_editor_property(p).export_text() if hasattr(source.get_editor_property(p), "export_text")
                else source.get_editor_property(p)) for p in properties)
    return snapshot


def build_child(parent, name, source):
    child = find_child(parent, name)
    if child is not None and child.get_class() != source.get_class():
        raise RuntimeError("child %s has class %s" % (name, child.get_class().get_name()))
    if child is None:
        child = unreal.new_object(source.get_class(), outer=parent, name=name)
    for prop in BASE_PROPERTIES + (SETTINGS_PROPERTY[source.get_class().get_name()],):
        child.set_editor_property(prop, fresh_copy(source.get_editor_property(prop)))
    marker = child.get_editor_property("m_ability_marker")
    marker.set_editor_property("marker_effect", None)
    child.set_editor_property("m_ability_marker", marker)
    child.set_editor_property("m_ability_sound_settings", unreal.GlobalAbilitySoundSettings())
    # The parent pays; children cost nothing (as make_costs_radixite_only.py leaves composite children).
    costs = child.get_editor_property("m_ability_costs")
    unit_cost = costs.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", {})
    costs.set_editor_property("costs", unit_cost)
    child.set_editor_property("m_ability_costs", costs)
    return child


def drop_workshop(child, workshop_bp):
    settings = child.get_editor_property("m_building_drop_settings")
    payloads = list(settings.get_editor_property("payloads"))
    if not payloads:
        raise RuntimeError(BUILDING_SOURCE + " has no payload")
    payload = fresh_copy(payloads[0])
    payload.set_editor_property("building_class", workshop_bp.generated_class())
    payload.set_editor_property("landing_offset", unreal.Vector2D(0.0, 0.0))
    descent = payload.get_editor_property("descent")
    descent.set_editor_property("canopy_scale", WORKSHOP_CANOPY_SCALE)
    descent.set_editor_property("canopy_count", WORKSHOP_CANOPY_COUNT)
    payload.set_editor_property("descent", descent)
    settings.set_editor_property("payloads", [payload])
    child.set_editor_property("m_building_drop_settings", settings)


def paradrop_two_pak40(child):
    settings = child.get_editor_property("m_paratroopers_settings")
    settings.set_editor_property("squad_subtypes", [member(unreal.SquadSubtype, "Squad_Ger_PaK40")] * 2)
    settings.set_editor_property("squad_spawn_ring_radius", TWO_GUN_RING_RADIUS)
    child.set_editor_property("m_paratroopers_settings", settings)


def author_template(asset, templates, ga_enum, workshop_bp, report):
    key = member(ga_enum, KEY)
    parent = templates.get(key)
    if parent is not None and parent.get_class() != unreal.GA_CompositeDrop.static_class():
        raise RuntimeError("%s exists with class %s" % (KEY, parent.get_class().get_name()))
    if parent is None:
        parent = unreal.new_object(unreal.GA_CompositeDrop, outer=asset, name="GA_Composite_" + KEY)
        report.append("created " + KEY)
    building_source = templates[member(ga_enum, BUILDING_SOURCE)]
    gun_source = templates[member(ga_enum, GUN_SOURCE)]
    parent.set_editor_property("m_ability_type", key)
    parent.set_editor_property("m_ability_tier", member(unreal.GlobalAbilityTier, TIER))
    parent.set_editor_property("m_aim_settings", fresh_copy(building_source.get_editor_property("m_aim_settings")))
    parent.set_editor_property("m_ability_marker", fresh_copy(building_source.get_editor_property("m_ability_marker")))
    parent.set_editor_property("m_ability_sound_settings",
                               fresh_copy(building_source.get_editor_property("m_ability_sound_settings")))

    cost_state = parent.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", {member(unreal.RTSResourceType, "Resource_Radixite"): COST_RADIXITE})
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", COOLDOWN)
    cost_state.set_editor_property("cool_down_remaining", 0)
    parent.set_editor_property("m_ability_costs", cost_state)

    ui = parent.get_editor_property("m_ui_settings")
    texture = unreal.load_asset(ICON_DIR + ICON)
    if texture is None:
        raise RuntimeError("icon %s missing" % ICON)
    ui.set_editor_property("ability_icon", texture)
    ui.set_editor_property("title", "<Text_Title>%s</>" % TITLE)
    ui.set_editor_property("description", DESCRIPTION)
    parent.set_editor_property("m_ui_settings", ui)

    workshop_child = build_child(parent, "Child_WorkshopDrop", building_source)
    drop_workshop(workshop_child, workshop_bp)
    gun_child = build_child(parent, "Child_Pak40Paradrop", gun_source)
    paradrop_two_pak40(gun_child)

    entries = []
    for child, delay, offset in ((workshop_child, 0.0, (0.0, 0.0)), (gun_child, GUN_DELAY, GUN_OFFSET)):
        entry = unreal.CompositeDropChild()
        entry.set_editor_property("ability", child)
        entry.set_editor_property("start_delay_seconds", delay)
        entry.set_editor_property("target_offset", unreal.Vector2D(offset[0], offset[1]))
        entry.set_editor_property("aim_mode", unreal.CompositeDropChildAim.POINT)
        entries.append(entry)
    settings = parent.get_editor_property("m_composite_drop_settings")
    settings.set_editor_property("children", entries)
    parent.set_editor_property("m_composite_drop_settings", settings)
    templates[key] = parent


def describe(parent):
    lines = ["%s tier=%s costs=%s cd=%d" % (
        KEY, parent.get_editor_property("m_ability_tier"),
        parent.get_editor_property("m_ability_costs").get_editor_property("costs").get_editor_property(
            "resource_costs"),
        parent.get_editor_property("m_ability_costs").get_editor_property("cool_down_time"))]
    for index, entry in enumerate(parent.get_editor_property("m_composite_drop_settings").get_editor_property(
            "children")):
        child = entry.get_editor_property("ability")
        prop = SETTINGS_PROPERTY.get(child.get_class().get_name())
        lines.append("  child %d %s outer=%s delay=%.1f offset=%s settings=%s" % (
            index, child.get_path_name(), child.get_outer().get_name(),
            entry.get_editor_property("start_delay_seconds"), entry.get_editor_property("target_offset"),
            child.get_editor_property(prop).export_text()[:600]))
    return lines


def run():
    report = []
    workshop_bp = build_workshop(report)
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    # UGlobalAbility and EGlobalAbility share a python name, so the enum type comes from the map keys.
    ga_enum = type(next(iter(templates.keys())))
    report.append("%d entries before" % len(templates))
    before = source_snapshot(templates, ga_enum)
    author_template(asset, templates, ga_enum, workshop_bp, report)
    asset.set_editor_property("m_ability_templates_by_type", templates)
    if not save_with_retry(asset, "DA_GlobalAbilitySystem"):
        return report + ["ERROR data asset not saved"]
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    report.append("saved entries=%d" % len(check))
    after = source_snapshot(check, ga_enum)
    for key_name in SOURCES:
        report.append("SOURCE %s %s" % ("UNCHANGED" if after[key_name] == before[key_name] else "CHANGED!", key_name))
    report += describe(check[member(ga_enum, KEY)])
    return report


try:
    for out_line in run():
        log(out_line)
except Exception as error:
    log("ERROR " + str(error))
    raise
