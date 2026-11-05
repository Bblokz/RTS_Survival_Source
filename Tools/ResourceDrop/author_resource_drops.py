"""Authors the three resource drop templates (UGA_ResourceDrop) in DA_GlobalAbilitySystem:
GA_GerRadixiteMetalSupplyDrop, GA_GerConstructionSupplyDrop and GA_GerRadixiteCacheDrop. Touches only these keys.

Run headless with a DebugGame build that has UGA_ResourceDrop:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Idempotent: re-running updates the existing templates. Saving the data asset from an editor whose DLL lacks
UGA_ResourceDrop drops these templates, so never save the DA from the shared Development editor until it is rebuilt.
Strip flight, aim and marker settings are copied from GA_GerAirMinefield (Me 410 transport, strip aim) on creation.
Design and numbers: Source/Docs/Abilities/Commanders.md, test: -ResourceDropValidate.
"""
import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG_PREFIX = "[ResourceDrop] "
SOURCE_TEMPLATE = "GA_GerAirMinefield"

PARACHUTE = "/Game/RTS_Survival/SpecialEffects/Flare/SM_Flare_Parachute"
DROP_POINT_MATERIAL = "/Game/RTS_Survival/Materials/AimMaterials/TargetAreas/M_DropPointGhost"
CONTAINER_CLASS = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/BP_ScavengeableObject.BP_ScavengeableObject_C"
DEPOSIT_CLASS = ("/Game/RTS_Survival/Blueprints/Resources/ChildBps/Radixite/Resource_Radixite_Crystal_NoDecal."
                 "Resource_Radixite_Crystal_NoDecal_C")
ICON_DIR = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/"

# Each entry: enum name, UI, balance, payload. Rewards are per container BEFORE the squad's scavenge multiplier:
# Scavengers and Rail Troopers triple them (Sturm Pioniere 1x), and scavenging rounds the result to 5. The numbers are
# tuned for one Scavenger squad: 1.5x the cost back, paid back after the third crate (~3 min after the cast).
TEMPLATES = [
    {
        "key": "GA_GerRadixiteMetalSupplyDrop",
        "title": "Radixite & Metal Supply Drop",
        "description": ("A transport flies the line you draw and drops 4 supply crates on parachutes. Scavengers "
                        "empty a crate in 50 seconds for 90 radixite and 60 metal: 360 radixite and 240 metal in "
                        "all. Other infantry with scavenging gear gets a third. Crates stay until emptied."),
        "icon": "T_GlobalAbility_GerAirMinefield",
        "radixite_cost": 400,
        "cooldown": 90,
        "kind": "SCAVENGE_CONTAINER",
        "count": 4,
        "falling_mesh": "/Game/MilitaryBaseCamp/Meshes/SM_CrateBig",
        "falling_scale": 1.5,
        "parachute_scale": 3.0,
        "rewards": {"RESOURCE_RADIXITE": 30, "RESOURCE_METAL": 20},
        "scavenge_seconds": 50.0,
        "strip": (1200.0, 3000.0, 2000.0, 600.0),
    },
    {
        "key": "GA_GerConstructionSupplyDrop",
        "title": "Construction Supply Drop",
        "description": ("A transport flies the line you draw and drops 4 bundles of construction material on "
                        "parachutes. Scavengers empty a bundle in 50 seconds for 120 metal and 30 radixite: 480 "
                        "metal and 120 radixite in all. Other infantry with scavenging gear gets a third."),
        "icon": "T_GlobalAbility_GerAirMinefield",
        "radixite_cost": 400,
        "cooldown": 90,
        "kind": "SCAVENGE_CONTAINER",
        "count": 4,
        "falling_mesh": "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/Misc/SM_ScavObj_MetalDump",
        "falling_scale": 0.55,
        "parachute_scale": 3.5,
        "rewards": {"RESOURCE_RADIXITE": 10, "RESOURCE_METAL": 40},
        "scavenge_seconds": 50.0,
        "strip": (1200.0, 3000.0, 2000.0, 600.0),
    },
    {
        "key": "GA_GerRadixiteCacheDrop",
        "title": "Radixite Cache Drop",
        "description": ("A transport drops a sealed radixite cache on a parachute halfway along the line you draw. "
                        "It cracks open into a crystal deposit of 1200 radixite that harvesters mine like any "
                        "other. Drop it close to a refinery and guard it."),
        "icon": "T_GlobalAbility_GerRadixitePzIIMarderDrop",
        "radixite_cost": 500,
        "cooldown": 90,
        "kind": "HARVESTABLE_DEPOSIT",
        "count": 1,
        "falling_mesh": "/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteCores/SM_RadixiteCore_1",
        "falling_scale": 3.0,
        "parachute_scale": 3.0,
        "deposit_amount": 1200,
        "deposit_scale": 1.0,
        "strip": (1200.0, 2000.0, 1500.0, 400.0),
    },
]

COPIED_ON_CREATE = ("m_ability_requirements", "m_aim_settings", "m_ability_sound_settings", "m_ability_marker",
                    "m_strip_flight_settings")


def pythonize(ue_name):
    """Unreal's Python name for an enum value (see Tools/Commanders/author_commander_abilities.py)."""
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


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("missing asset " + path)
    return asset


def apply_ui_and_balance(obj, spec):
    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("title", "<Text_Title>%s</>" % spec["title"])
    ui.set_editor_property("description", spec["description"])
    ui.set_editor_property("ability_icon", load(ICON_DIR + spec["icon"]))
    obj.set_editor_property("m_ui_settings", ui)
    obj.set_editor_property("m_ability_tier", unreal.GlobalAbilityTier.TIER_I)
    cost_state = obj.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", {unreal.RTSResourceType.RESOURCE_RADIXITE: spec["radixite_cost"]})
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", spec["cooldown"])
    obj.set_editor_property("m_ability_costs", cost_state)


def apply_aim(obj, spec):
    min_length, max_length, before_anchor, radius = spec["strip"]
    aim = obj.get_editor_property("m_aim_settings")
    area = aim.get_editor_property("area_aim")
    area.set_editor_property("shape", unreal.GlobalAbilityAimShape.STRIP)
    area.set_editor_property("area_radius", radius)
    area.set_editor_property("min_strip_length", min_length)
    area.set_editor_property("max_strip_length", max_length)
    area.set_editor_property("length_before_anchor", before_anchor)
    area.set_editor_property("shader_marker_count", 0)
    area.set_editor_property("drop_point_mesh", load(spec["falling_mesh"]))
    area.set_editor_property("drop_point_material", load(DROP_POINT_MATERIAL))
    area.set_editor_property("drop_point_scale", spec["falling_scale"])
    aim.set_editor_property("area_aim", area)
    obj.set_editor_property("m_aim_settings", aim)


def apply_payload(obj, spec):
    settings = obj.get_editor_property("m_resource_drop_settings")
    settings.set_editor_property("payload_kind", getattr(unreal.ResourceDropPayloadKind, spec["kind"]))
    settings.set_editor_property("payload_count", spec["count"])
    fall = settings.get_editor_property("fall_settings")
    fall.set_editor_property("falling_mesh", load(spec["falling_mesh"]))
    fall.set_editor_property("falling_mesh_scale", spec["falling_scale"])
    fall.set_editor_property("parachute_mesh", load(PARACHUTE))
    fall.set_editor_property("parachute_scale", spec["parachute_scale"])
    fall.set_editor_property("fall_seconds", 6.0)
    settings.set_editor_property("fall_settings", fall)
    if spec["kind"] == "SCAVENGE_CONTAINER":
        container = settings.get_editor_property("container_settings")
        container.set_editor_property("container_class", unreal.load_class(None, CONTAINER_CLASS))
        container.set_editor_property("container_mesh", load(spec["falling_mesh"]))
        container.set_editor_property("container_mesh_scale", spec["falling_scale"])
        container.set_editor_property("rewards_per_container",
                                      {getattr(unreal.RTSResourceType, k): v for k, v in spec["rewards"].items()})
        container.set_editor_property("scavenge_seconds", spec["scavenge_seconds"])
        # 0: the shared auto-scavenge trigger re-orders squads busy on a neighbouring crate (see report).
        container.set_editor_property("auto_scavenge_radius", 0.0)
        container.set_editor_property("remove_after_emptied_seconds", 7.0)
        settings.set_editor_property("container_settings", container)
    else:
        deposit = settings.get_editor_property("deposit_settings")
        deposit.set_editor_property("deposit_class", unreal.load_class(None, DEPOSIT_CLASS))
        deposit.set_editor_property("deposit_amount", spec["deposit_amount"])
        deposit.set_editor_property("deposit_actor_scale", spec["deposit_scale"])
        settings.set_editor_property("deposit_settings", deposit)
    obj.set_editor_property("m_resource_drop_settings", settings)


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    # UGlobalAbility and EGlobalAbility share a python name, so the enum type comes from the map keys.
    enum_type = type(next(iter(templates.keys())))
    report, errors = ["%d entries before" % len(templates)], []
    source = templates.get(member(enum_type, SOURCE_TEMPLATE))
    for spec in TEMPLATES:
        try:
            key = member(enum_type, spec["key"])
            obj = templates.get(key)
            if obj is not None and obj.get_class() != unreal.GA_ResourceDrop.static_class():
                raise RuntimeError("existing template is a %s" % obj.get_class().get_name())
            if obj is None:
                if source is None:
                    raise RuntimeError("source template %s missing" % SOURCE_TEMPLATE)
                obj = unreal.new_object(unreal.GA_ResourceDrop, outer=asset, name="GA_ResourceDrop_" + spec["key"])
                for prop in COPIED_ON_CREATE:
                    obj.set_editor_property(prop, source.get_editor_property(prop))
                report.append("created %s" % spec["key"])
            obj.set_editor_property("m_ability_type", key)
            apply_ui_and_balance(obj, spec)
            apply_aim(obj, spec)
            apply_payload(obj, spec)
            templates[key] = obj
        except Exception as error:
            errors.append("%s: %s" % (spec["key"], error))
    if errors:
        return report + ["ERROR " + e for e in errors] + ["NOT SAVED"]
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    report.append("saved=%s entries=%d" % (saved, len(templates)))
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    for spec in TEMPLATES:
        obj = check.get(member(enum_type, spec["key"]))
        if obj is None:
            report.append("VERIFY %s missing after save" % spec["key"])
            continue
        report.append("VERIFY %s class=%s tier=%s costs=%s drop=%s" % (
            spec["key"], obj.get_class().get_name(), obj.get_editor_property("m_ability_tier"),
            obj.get_editor_property("m_ability_costs").export_text(),
            obj.get_editor_property("m_resource_drop_settings").export_text()[:600]))
    return report


for line in run():
    unreal.log(LOG_PREFIX + line)
