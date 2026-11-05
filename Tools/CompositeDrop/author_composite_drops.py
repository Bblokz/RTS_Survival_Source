"""Authors the composite (several aircraft, several delivery types) templates in DA_GlobalAbilitySystem:
GA_GerLuftlandeSturm and GA_GerMarderScavengerDrop, both UGA_CompositeDrop.

Run headless with a DebugGame editor DLL that knows UGA_CompositeDrop:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Hold Saved/AgentLocks/da_globalabilities.lockdir while it runs. Saving the data asset from an editor whose DLL lacks
UGA_CompositeDrop drops these templates.

Idempotent: touches only the two keys below, reuses the existing parent and child objects, and re-copies every child
from its source template each run, so a change to a source (aircraft, parachute) is picked up by re-running.
Saves only when every step succeeded. Design: Source/Docs/Abilities/Commanders.md (cost bands).

Data layout (author more composites the same way):
  parent  = unreal.GA_CompositeDrop, outer = the data asset; owns tier, costs, cooldown, aim, UI, marker.
  parent.m_composite_drop_settings.children = [unreal.CompositeDropChild(
      ability=<any GlobalAbility object, outer = the PARENT>, start_delay_seconds=float,
      target_offset=unreal.Vector2D(x, y)  # world cm from the aimed point
      aim_mode=unreal.CompositeDropChildAim.POINT | STRIP_ENDING_AT_TARGET | STRIP_CENTRED_ON_TARGET)]
  Children must be outered to the parent so the manager's DuplicateObject copies them with it. Clear their marker
  (the parent's beacon is enough) and sounds; their own cost/cooldown/tier/UI are ignored.
"""
import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
ICON_DIR = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/"
LOG_PREFIX = "[CompositeDrop] "

# Properties every child copies from its source template; class settings are added per child.
BASE_PROPERTIES = ("m_ability_type", "m_aim_settings")
SETTINGS_PROPERTY = {
    "GA_AircraftStrafing": "m_aircraft_strafing_settings",
    "GA_AircraftDrop": "m_aircraft_drop_settings",
    "GA_Paratroopers": "m_paratroopers_settings",
    "GA_BuildingParachuteDrop": "m_building_drop_settings",
}

# Luftlande-Sturm timing. STRIKE_SECONDS is the strafing TotalStrafeTime: it counts from the order (approach
# included); when it runs out the Ju 87s stop firing and fly off. With enemies at the target they keep diving on them
# until then. The Ju 390 starts PARADROP_DELAY seconds after the click; the first paratrooper touches down
# PARADROP_DELAY + its approach + ~4.5 s (jump + descent) later. Keep PARADROP_DELAY >= STRIKE_SECONDS - 4 so nobody
# lands while the Ju 87s may still fire, on any map size (both fly in from the map edge).
# STRIKE_SECONDS also has to cover the Ju 87s' approach: on RTS_ToCopySmall the far one fired after ~17.5 s.
# -CompositeDropValidate checks first touchdown > STRIKE_SECONDS.
# Known upstream issue: a strafing Ju 87 that spawns outside its dive range only attacks enemy units (after its
# initial run it turns aggressive); on empty ground it circles without firing (plain GA_Ju87Strafing does the same).
STRIKE_SECONDS = 35.0
PARADROP_DELAY = 31.0
# Paradrop + gun/pit composites: the second plane drops 25 m to the side, clear of the infantry's 7.5 m landing
# spiral and of the guns' 6.5 m spawn ring; it starts 3 s later so the two transports do not fly into each other.
SIDE_DROP_OFFSET = (0.0, 2500.0)
SIDE_DROP_DELAY = 3.0
# Two guns: the Flak 88 drop's 2 m ring is for one gun; the howitzer battery uses 6.5 m for two.
TWO_GUN_RING_RADIUS = 650.0
# Marder drop: the scavengers land 30 m to the side. The landed Ju 390 (50 m span, 34 m long) cuts a hole in the
# navmesh around the target, and a closer paradrop gets pushed out of it to wherever the navmesh is nearest.


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
    for candidate in (pythonize(ue_name), ue_name.upper()):
        value = getattr(enum_cls, candidate, None)
        if value is not None:
            return value
    raise KeyError("%s has no member %s (tried %s)" % (enum_cls.__name__, ue_name, pythonize(ue_name)))


R = member(unreal.RTSResourceType, "Resource_Radixite")
VP = member(unreal.RTSResourceType, "Resource_VehicleParts")
M = member(unreal.RTSResourceType, "Resource_Metal")


def squad(name):
    return member(unreal.SquadSubtype, name)


# ---------------------------------------------------------------- child edits

def strike_two_ju87(child):
    settings = child.get_editor_property("m_aircraft_strafing_settings")
    classes = list(settings.get_editor_property("aircraft_classes"))
    if len(classes) < 2:
        raise RuntimeError("GA_3xJu87Strafing has %d aircraft classes" % len(classes))
    settings.set_editor_property("aircraft_classes", classes[:2])
    settings.set_editor_property("total_strafe_time", STRIKE_SECONDS)
    child.set_editor_property("m_aircraft_strafing_settings", settings)


def paradrop_two_vultures(child):
    settings = child.get_editor_property("m_paratroopers_settings")
    settings.set_editor_property("squad_subtypes", [squad("Squad_Ger_Vultures")] * 2)
    child.set_editor_property("m_paratroopers_settings", settings)


def land_one_marder(child):
    settings = child.get_editor_property("m_aircraft_drop_settings")
    # The source lands two radixite Pz II F and the dual-barrel Marder override; keep only the Marder entry.
    marders = [entry for entry in settings.get_editor_property("tanks_with_offsets")
               if "Ger_Vehicle_Marder_DualBarrel" in entry.export_text()]
    if not marders:
        raise RuntimeError("no Marder_DualBarrel entry in GA_GerRadixitePzIIMarderDrop")
    settings.set_editor_property("tanks_with_offsets", marders[:1])
    settings.set_editor_property("squad_subtypes", [])
    child.set_editor_property("m_aircraft_drop_settings", settings)


def paradrop_one_scavenger_squad(child):
    settings = child.get_editor_property("m_paratroopers_settings")
    settings.set_editor_property("squad_subtypes", [squad("Squad_Ger_Scavengers")])
    child.set_editor_property("m_paratroopers_settings", settings)


def paradrop_two_elite_scavenger_squads(child):
    settings = child.get_editor_property("m_paratroopers_settings")
    settings.set_editor_property("squad_subtypes", [squad("Squad_Ger_EliteScavengers")] * 2)
    child.set_editor_property("m_paratroopers_settings", settings)


def paradrop_two_pak40(child):
    # Flak 88 drop settings (gun on a cargo chute, crew jumps, transport at >= 50 m) with two Pak 40 squads.
    settings = child.get_editor_property("m_paratroopers_settings")
    settings.set_editor_property("squad_subtypes", [squad("Squad_Ger_PaK40")] * 2)
    settings.set_editor_property("squad_spawn_ring_radius", TWO_GUN_RING_RADIUS)
    child.set_editor_property("m_paratroopers_settings", settings)


def paradrop_two_sturm_pionieren(child):
    settings = child.get_editor_property("m_paratroopers_settings")
    settings.set_editor_property("squad_subtypes", [squad("Squad_Ger_SturmPionieren")] * 2)
    child.set_editor_property("m_paratroopers_settings", settings)


def keep_pak_pit(child):
    """GA_GerPakPitDrop as it is: one Pak 40 pit under one giant canopy."""


# ------------------------------------------------------------- composite table
# (key, title, description, tier, costs, cooldown, icon, aim source, marker source, children)
# Costs are radixite only (user decision 2026-11-05); the children stay cost-free, the parent pays.
# child: (object name, source key, edit function, start delay, offset (x, y), aim mode)
COMPOSITES = [
    ("GA_GerLuftlandeSturm", "Luftlande-Sturm",
     "Two Ju 87 dive-bomb the target; once the dives are over, a Ju 390 paradrops two Vulture squads into the "
     "strike zone.",
     "TIER_III", {R: 1700}, 240,
     "T_GlobalAbility_3xJu87Strafing", "GA_3xJu87Strafing", "GA_GerParatroopers",
     [("Child_Ju87Strike", "GA_3xJu87Strafing", strike_two_ju87, 0.0, (0.0, 0.0), "POINT"),
      ("Child_VulturesParadrop", "GA_GerParatroopers", paradrop_two_vultures, PARADROP_DELAY, (0.0, 0.0), "POINT")]),
    ("GA_GerMarderScavengerDrop", "Marder and Scavenger Drop",
     "A Ju 390 lands a dual-barrel Marder at the target while a second transport paradrops a scavenger squad "
     "beside it.",
     "TIER_I", {R: 550}, 90,
     "T_GlobalAbility_GerRadixitePzIIMarderDrop", "GA_GerRadixitePzIIMarderDrop", "GA_GerRadixitePzIIMarderDrop",
     [("Child_MarderLanding", "GA_GerRadixitePzIIMarderDrop", land_one_marder, 0.0, (0.0, 0.0), "POINT"),
      ("Child_ScavengerParadrop", "GA_GerParatroopers", paradrop_one_scavenger_squad, 2.0, (0.0, 3000.0), "POINT")]),
    # Scrap Warlord slot 4. 2x Elite Scavengers (450 R + 125 M) + 2x Pak 40 (450 R + 180 M) = 1800 R + 610 M, x1.25.
    ("GA_GerEliteScavengersPakDrop", "Elite Scavengers and Pak 40 Drop",
     "A Ju 390 paradrops two Elite Scavenger squads on the target while a second transport drops two Pak 40 guns on "
     "cargo chutes beside them, their crews jumping after them.",
     "TIER_II", {R: 2250}, 180,
     "T_GlobalAbility_GerParatroopers", "GA_GerEliteScavengersParadrop", "GA_GerEliteScavengersParadrop",
     [("Child_EliteScavengersParadrop", "GA_GerEliteScavengersParadrop", paradrop_two_elite_scavenger_squads, 0.0,
       (0.0, 0.0), "POINT"),
      ("Child_Pak40Paradrop", "GA_GerFlak88Drop", paradrop_two_pak40, SIDE_DROP_DELAY, SIDE_DROP_OFFSET, "POINT")]),
    # Festungsbaumeister slot 4. 2x Sturm Pionieren (400 R + 200 M) + Pak 40 pit (380 R + 200 M) = 1180 R + 600 M,
    # x1.25.
    ("GA_GerSturmPionierenPakPitDrop", "Sturm Pionieren and Pak Pit Drop",
     "A Ju 390 paradrops two Sturm Pionieren squads on the target while a second transport drops a complete Pak 40 "
     "gun pit under a giant canopy beside them, ready to fire when it lands.",
     "TIER_II", {R: 1475}, 180,
     "T_GlobalAbility_GerSpotterPzIIIDrop", "GA_GerEliteScavengersParadrop", "GA_GerEliteScavengersParadrop",
     [("Child_SturmPionierenParadrop", "GA_GerEliteScavengersParadrop", paradrop_two_sturm_pionieren, 0.0,
       (0.0, 0.0), "POINT"),
      ("Child_PakPitDrop", "GA_GerPakPitDrop", keep_pak_pit, SIDE_DROP_DELAY, SIDE_DROP_OFFSET, "POINT")]),
]

# Every template the children or parents copy from; the script checks they are byte-for-byte unchanged after saving.
SOURCE_PROPERTIES = BASE_PROPERTIES + ("m_ability_marker", "m_ability_sound_settings", "m_ability_costs",
                                       "m_ui_settings", "m_ability_tier")


def find_child(parent, name):
    return unreal.find_object(None, "%s.%s" % (parent.get_path_name(), name))


def fresh_copy(value):
    """A struct read from the source rebuilt from its text, so nothing written later can reach the source template
    (the wrapper get_editor_property returns can alias the source's memory)."""
    if not hasattr(value, "export_text") or not hasattr(value, "import_text"):
        return value
    copy = type(value)()
    copy.import_text(value.export_text())
    return copy


def source_snapshot(templates, ga_enum):
    keys = set()
    for entry in COMPOSITES:
        keys.update((entry[7], entry[8]))
        keys.update(child[1] for child in entry[9])
    snapshot = {}
    for key_name in sorted(keys):
        source = templates[member(ga_enum, key_name)]
        properties = SOURCE_PROPERTIES + (SETTINGS_PROPERTY.get(source.get_class().get_name(), "m_ability_type"),)
        snapshot[key_name] = "|".join(str(source.get_editor_property(prop).export_text()
                                          if hasattr(source.get_editor_property(prop), "export_text")
                                          else source.get_editor_property(prop)) for prop in properties)
    return snapshot


def build_child(parent, name, source, edit):
    child = find_child(parent, name)
    if child is not None and child.get_class() != source.get_class():
        raise RuntimeError("child %s has class %s, source %s" % (name, child.get_class().get_name(),
                                                                   source.get_class().get_name()))
    if child is None:
        child = unreal.new_object(source.get_class(), outer=parent, name=name)
    settings_property = SETTINGS_PROPERTY.get(source.get_class().get_name())
    if settings_property is None:
        raise RuntimeError("no settings property known for %s" % source.get_class().get_name())
    for prop in BASE_PROPERTIES + (settings_property,):
        child.set_editor_property(prop, fresh_copy(source.get_editor_property(prop)))
    marker = child.get_editor_property("m_ability_marker")
    marker.set_editor_property("marker_effect", None)
    child.set_editor_property("m_ability_marker", marker)
    child.set_editor_property("m_ability_sound_settings", unreal.GlobalAbilitySoundSettings())
    edit(child)
    return child


def author(asset, templates, ga_enum, entry, report):
    key_name, title, description, tier, costs, cooldown, icon, aim_source, marker_source, children = entry
    key = member(ga_enum, key_name)
    parent = templates.get(key)
    if parent is not None and parent.get_class() != unreal.GA_CompositeDrop.static_class():
        raise RuntimeError("%s exists with class %s" % (key_name, parent.get_class().get_name()))
    if parent is None:
        parent = unreal.new_object(unreal.GA_CompositeDrop, outer=asset, name="GA_Composite_" + key_name)
        report.append("created %s" % key_name)
    parent.set_editor_property("m_ability_type", key)
    parent.set_editor_property("m_ability_tier", member(unreal.GlobalAbilityTier, tier))
    parent.set_editor_property("m_aim_settings", fresh_copy(templates[member(ga_enum, aim_source)].get_editor_property(
        "m_aim_settings")))
    parent.set_editor_property("m_ability_marker", fresh_copy(
        templates[member(ga_enum, marker_source)].get_editor_property("m_ability_marker")))

    cost_state = parent.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", dict(costs))
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", cooldown)
    cost_state.set_editor_property("cool_down_remaining", 0)
    parent.set_editor_property("m_ability_costs", cost_state)

    ui = parent.get_editor_property("m_ui_settings")
    texture = unreal.load_asset(ICON_DIR + icon)
    if texture is None:
        raise RuntimeError("icon %s missing" % icon)
    ui.set_editor_property("ability_icon", texture)
    ui.set_editor_property("title", "<Text_Title>%s</>" % title)
    ui.set_editor_property("description", description)
    parent.set_editor_property("m_ui_settings", ui)

    entries = []
    for name, source_name, edit, delay, offset, aim_mode in children:
        source = templates.get(member(ga_enum, source_name))
        if source is None:
            raise RuntimeError("source template %s missing" % source_name)
        child = build_child(parent, name, source, edit)
        entry_struct = unreal.CompositeDropChild()
        entry_struct.set_editor_property("ability", child)
        entry_struct.set_editor_property("start_delay_seconds", delay)
        entry_struct.set_editor_property("target_offset", unreal.Vector2D(offset[0], offset[1]))
        entry_struct.set_editor_property("aim_mode", getattr(unreal.CompositeDropChildAim, aim_mode))
        entries.append(entry_struct)
    settings = parent.get_editor_property("m_composite_drop_settings")
    settings.set_editor_property("children", entries)
    parent.set_editor_property("m_composite_drop_settings", settings)
    templates[key] = parent
    report.append("authored %s: %d children, %s, cd=%d" % (key_name, len(entries), tier, cooldown))


def describe(parent):
    lines = []
    for index, entry in enumerate(parent.get_editor_property("m_composite_drop_settings").get_editor_property(
            "children")):
        child = entry.get_editor_property("ability")
        settings_property = SETTINGS_PROPERTY.get(child.get_class().get_name()) if child else None
        lines.append("  child %d %s outer=%s delay=%.1f offset=%s aim=%s settings=%s" % (
            index, child.get_path_name() if child else None, child.get_outer().get_name() if child else None,
            entry.get_editor_property("start_delay_seconds"), entry.get_editor_property("target_offset"),
            entry.get_editor_property("aim_mode"),
            child.get_editor_property(settings_property).export_text()[:400] if settings_property else None))
    return lines


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    # UGlobalAbility and EGlobalAbility share a python name, so the enum type comes from the map keys.
    ga_enum = type(next(iter(templates.keys())))
    report, errors = ["%d entries before" % len(templates)], []
    sources_before = source_snapshot(templates, ga_enum)
    for entry in COMPOSITES:
        try:
            author(asset, templates, ga_enum, entry, report)
        except Exception as error:
            errors.append("%s: %s" % (entry[0], error))
    if errors:
        return report + ["ERROR " + e for e in errors] + ["NOT SAVED"]
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    report.append("saved=%s entries=%d" % (saved, len(templates)))
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    sources_after = source_snapshot(check, ga_enum)
    for key_name, before in sources_before.items():
        report.append("SOURCE %s %s" % ("UNCHANGED" if sources_after[key_name] == before else "CHANGED!", key_name))
        source = check[member(ga_enum, key_name)]
        settings_property = SETTINGS_PROPERTY.get(source.get_class().get_name())
        if settings_property:
            text = source.get_editor_property(settings_property).export_text()
            report.append("  %s: %s" % (key_name, text))
    for entry in COMPOSITES:
        parent = check[member(ga_enum, entry[0])]
        costs = parent.get_editor_property("m_ability_costs")
        report.append("%s tier=%s cd=%d costs=%s" % (
            entry[0], parent.get_editor_property("m_ability_tier"), costs.get_editor_property("cool_down_time"),
            costs.get_editor_property("costs").get_editor_property("resource_costs")))
        report += describe(parent)
    return report


for line in run():
    unreal.log(LOG_PREFIX + line)
