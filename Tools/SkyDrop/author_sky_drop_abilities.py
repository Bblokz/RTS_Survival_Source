"""Authors the Sky-Drop Quartermaster's own templates in DA_GlobalAbilitySystem (round 3d, 2026-11-05):
  GA_GerMassiveFlare            UGA_FlareCurtain copy of GA_GerFlareCurtain: one huge, long-burning flare, circle aim
  GA_GerPumaDrop                UGA_AircraftDrop copy of GA_GerSpotterPzIIIDrop: a Ju 390 lands one Puma
  GA_GerVehiclePartsSupplyDrop  UGA_ResourceDrop copy of GA_GerRadixiteMetalSupplyDrop: crates of vehicle parts
  GA_GerFlakLuchsStugDrop       existing: re-tiered to Tier II (tier, radixite cost, cooldown only)
Touches only these four keys; every other template is fingerprinted before and after the save and the run reports
any difference. Costs are radixite only (user decision 2026-11-05).

The data asset holds templates of classes the Development DLL lacks, so run it headless on the DebugGame build under
Saved/AgentLocks/da_globalabilities.lockdir and debuggame_build.lockdir:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Idempotent: an existing template is updated in place (its class must match). New templates copy every setting of
their source through export_text/import_text (a struct from get_editor_property can alias the source's memory).
Report: Saved/SkyDropTest/author_sky_drop_<time>.txt and the log ([SkyDrop] prefix).
Design and numbers: Source/Docs/Abilities/Commanders.md. Test: -GlobalAbilityAreaValidate -GlobalAbilitySkyDrop.
"""
import os
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG_PREFIX = "[SkyDrop] "
SAVE_RETRIES = 12
SAVE_RETRY_SECONDS = 10.0

PUMA_CLASS = ("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Puma/"
              "Ger_ArmoredCar_Puma.Ger_ArmoredCar_Puma_C")
VEHICLE_PARTS_MESH = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/Meshes/SM_ScavObj_VehicleParts1"

# Massive flare, relative to one flare of the curtain (its source): far wider vision and light, a bigger flare that
# burns much longer. The ignition height stays: it must hang below the RTS camera.
FLARE_RADIUS_SCALE = 3.0
FLARE_LIGHT_INTENSITY_SCALE = 4.0
FLARE_MESH_SCALE = 2.5
FLARE_HANG_SECONDS = 75.0
# One click; the strip the aircraft flies is centred on the point and the single flare hangs over its middle.
FLARE_STRIP_LENGTH = 1000.0

# Vehicle parts crates: 4 crates of 25 parts before the squad multiplier (Scavengers x3: 75 per crate, 300 in all).
# At ~2 radixite per vehicle part (unit costs: Puma 180 R + 80 VP, StuG 300 R + 125 VP) that is 600 radixite worth for
# 400, 1.5x like the radixite & metal supply drop, paid back after the third crate (~3 min after the cast).
VEHICLE_PARTS_PER_CRATE = 25
SUPPLY_COST = 400
SUPPLY_COOLDOWN = 90
SUPPLY_SCAVENGE_SECONDS = 50.0

# 1.25x the radixite training cost, rounded to 25 (DeveloperSettings.h): Puma 180 R; Flak Luchs 240 R x2 + StuG 300 R.
PUMA_COST = 225
FLAK_LUCHS_STUG_COST = 975
TIER_I_COOLDOWN = 90
TIER_II_COOLDOWN = 150

MY_KEYS = ("GA_GerMassiveFlare", "GA_GerPumaDrop", "GA_GerVehiclePartsSupplyDrop", "GA_GerFlakLuchsStugDrop")

# Every template property a fingerprint looks at; missing ones are skipped per class.
FINGERPRINT_PROPERTIES = (
    "m_ability_type", "m_ability_tier", "m_ability_costs", "m_ability_requirements", "m_aim_settings",
    "m_ability_sound_settings", "m_ui_settings", "m_ability_marker", "m_aircraft_drop_settings",
    "m_aircraft_strafing_settings", "m_aircraft_bombing_settings", "m_barrage_settings", "m_glider_drop_settings",
    "m_paratroopers_settings", "m_strip_flight_settings", "m_flare_curtain_settings", "m_resource_drop_settings",
    "m_projectile_mover", "m_weapon_data", "m_weapon_vfx")

COPIED_PROPERTIES = (
    "m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings", "m_ui_settings",
    "m_ability_marker", "m_ability_tier", "m_aircraft_drop_settings", "m_strip_flight_settings",
    "m_flare_curtain_settings", "m_resource_drop_settings")


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
    wanted = ue_name.replace("_", "").lower()
    for member_name in dir(enum_cls):
        if member_name.replace("_", "").lower() == wanted:
            return getattr(enum_cls, member_name)
    raise KeyError("%s has no member %s (tried %s)" % (enum_cls.__name__, ue_name, pythonize(ue_name)))


GA_ENUM = None
R = member(unreal.RTSResourceType, "Resource_Radixite")
VP = member(unreal.RTSResourceType, "Resource_VehicleParts")


def ga(name):
    return member(GA_ENUM, name)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("missing asset " + path)
    return asset


def detached_copy(value):
    """A struct copy that shares no memory with the template it came from."""
    if not hasattr(value, "export_text"):
        return value
    text = value.export_text()
    copy = type(value)()
    copy.import_text(text)
    if copy.export_text() != text:
        raise RuntimeError("struct %s did not round-trip through text" % type(value).__name__)
    return copy


def fingerprint(obj):
    parts = [obj.get_class().get_name(), obj.get_name()]
    for prop in FINGERPRINT_PROPERTIES:
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        parts.append("%s=%s" % (prop, value.export_text() if hasattr(value, "export_text") else str(value)))
    return "|".join(parts)


def copy_props(source, target):
    copied = []
    for prop in COPIED_PROPERTIES:
        try:
            value = source.get_editor_property(prop)
        except Exception:
            continue
        target.set_editor_property(prop, detached_copy(value))
        copied.append(prop)
    return copied


def get_or_create(asset, templates, name, source_name, expected_class, report):
    key = ga(name)
    obj = templates.get(key)
    if obj is not None:
        if obj.get_class() != expected_class.static_class():
            raise RuntimeError("existing template is a %s" % obj.get_class().get_name())
        return obj
    source = templates.get(ga(source_name))
    if source is None or source.get_class() != expected_class.static_class():
        raise RuntimeError("source %s missing or not a %s" % (source_name, expected_class.__name__))
    obj = unreal.new_object(expected_class, outer=asset, name="GA_SkyDrop_" + name)
    copied = copy_props(source, obj)
    report.append("created %s from %s (%d properties)" % (name, source_name, len(copied)))
    return obj


def set_ui(obj, title, description):
    ui = detached_copy(obj.get_editor_property("m_ui_settings"))
    ui.set_editor_property("title", "<Text_Title>%s</>" % title)
    ui.set_editor_property("description", description)
    obj.set_editor_property("m_ui_settings", ui)


def radixite_of(obj):
    costs = obj.get_editor_property("m_ability_costs").get_editor_property("costs")
    return int(costs.get_editor_property("resource_costs").get(R, 0))


def find_group(text, field):
    """(start, end) of 'field=(...)' in an exported struct text, balanced parentheses; None when absent."""
    start = text.find(field + "=")
    if start < 0:
        return None
    depth = 0
    for end in range(start + len(field) + 1, len(text)):
        if text[end] == "(":
            depth += 1
        elif text[end] == ")":
            depth -= 1
            if depth == 0:
                return start, end + 1
    raise RuntimeError("unbalanced %s in %s" % (field, text))


def set_balance(obj, tier_name, radixite, cooldown):
    """FUnitCost::ResourceCosts is EditDefaultsOnly, so Python cannot set it on an instance: the cost state is
    rebuilt from its exported text with a radixite-only ResourceCosts (as Tools/Commanders/make_costs_radixite_only.py)."""
    obj.set_editor_property("m_ability_tier", member(unreal.GlobalAbilityTier, tier_name))
    cost_state = obj.get_editor_property("m_ability_costs")
    text = cost_state.export_text()
    new_group = "ResourceCosts=((Resource_Radixite, %d))" % radixite
    group = find_group(text, "ResourceCosts")
    if group is not None:
        new_text = text[:group[0]] + new_group + text[group[1]:]
    else:
        costs_group = find_group(text, "Costs")
        if costs_group is None or text[costs_group[0]:costs_group[1]] != "Costs=()":
            raise RuntimeError("unexpected cost text %s" % text)
        new_text = text[:costs_group[0]] + "Costs=(%s)" % new_group + text[costs_group[1]:]
    fresh = type(cost_state)()
    fresh.import_text(new_text)
    fresh.set_editor_property("cool_down_time", cooldown)
    fresh.set_editor_property("cool_down_remaining", 0)
    got = dict(fresh.get_editor_property("costs").get_editor_property("resource_costs"))
    if got != {R: radixite}:
        raise RuntimeError("cost import gave %s (text %s)" % (got, new_text))
    obj.set_editor_property("m_ability_costs", fresh)


def author_massive_flare(asset, templates, report):
    curtain = templates.get(ga("GA_GerFlareCurtain"))
    obj = get_or_create(asset, templates, "GA_GerMassiveFlare", "GA_GerFlareCurtain", unreal.GA_FlareCurtain, report)
    curtain_aim = curtain.get_editor_property("m_aim_settings").get_editor_property("area_aim")
    curtain_flare = curtain.get_editor_property("m_flare_curtain_settings")
    curtain_visuals = curtain_flare.get_editor_property("visuals")
    radius = round(curtain_aim.get_editor_property("area_radius") * FLARE_RADIUS_SCALE / 100.0) * 100.0

    aim = detached_copy(obj.get_editor_property("m_aim_settings"))
    area = aim.get_editor_property("area_aim")
    area.set_editor_property("shape", unreal.GlobalAbilityAimShape.CIRCLE)
    area.set_editor_property("area_radius", radius)
    area.set_editor_property("inner_radius", 0.0)
    area.set_editor_property("min_strip_length", FLARE_STRIP_LENGTH)
    area.set_editor_property("max_strip_length", FLARE_STRIP_LENGTH)
    area.set_editor_property("length_before_anchor", 0.0)
    area.set_editor_property("shader_marker_count", 1)
    aim.set_editor_property("area_aim", area)
    obj.set_editor_property("m_aim_settings", aim)

    flare = detached_copy(obj.get_editor_property("m_flare_curtain_settings"))
    flare.set_editor_property("ignition_height", curtain_flare.get_editor_property("ignition_height"))
    flare.set_editor_property("hang_time_seconds", FLARE_HANG_SECONDS)
    flare.set_editor_property("fall_speed", curtain_flare.get_editor_property("fall_speed"))
    visuals = flare.get_editor_property("visuals")
    visuals.set_editor_property("light_intensity",
                                curtain_visuals.get_editor_property("light_intensity") * FLARE_LIGHT_INTENSITY_SCALE)
    visuals.set_editor_property("light_attenuation_radius", radius)
    visuals.set_editor_property("mesh_scale", curtain_visuals.get_editor_property("mesh_scale") * FLARE_MESH_SCALE)
    flare.set_editor_property("visuals", visuals)
    obj.set_editor_property("m_flare_curtain_settings", flare)

    set_ui(obj, "Massive Flare",
           "A bomber drops one giant parachute flare over the point you click. It burns for %d seconds and lights "
           "up and reveals everything within %d metres: far more than any flare of a curtain." % (
               int(FLARE_HANG_SECONDS), int(radius / 100)))
    curtain_cost = radixite_of(curtain)
    set_balance(obj, "TierI", curtain_cost if curtain_cost > 0 else 200, TIER_I_COOLDOWN)
    report.append("massive flare: curtain radius=%s hang=%s light=%s/%s mesh=%s cost=%d -> radius=%s" % (
        curtain_aim.get_editor_property("area_radius"), curtain_flare.get_editor_property("hang_time_seconds"),
        curtain_visuals.get_editor_property("light_intensity"),
        curtain_visuals.get_editor_property("light_attenuation_radius"),
        curtain_visuals.get_editor_property("mesh_scale"), curtain_cost, radius))
    return obj


def set_class_override(struct, prop, class_path):
    for value in (unreal.load_class(None, class_path), unreal.SoftClassPath(class_path)):
        try:
            struct.set_editor_property(prop, value)
        except Exception:
            continue
        if class_path.split(".")[-1] in str(struct.get_editor_property(prop)):
            return
    raise RuntimeError("could not set %s to %s (still %s)" % (prop, class_path, struct.get_editor_property(prop)))


def author_puma_drop(asset, templates, report):
    spotter = templates.get(ga("GA_GerSpotterPzIIIDrop"))
    obj = get_or_create(asset, templates, "GA_GerPumaDrop", "GA_GerSpotterPzIIIDrop", unreal.GA_AircraftDrop, report)
    source_tanks = spotter.get_editor_property("m_aircraft_drop_settings").get_editor_property("tanks_with_offsets")
    entry = unreal.AircraftDropTankWithOffset()
    entry.set_editor_property("tank_subtype", member(unreal.TankSubtype, "Tank_Puma"))
    if len(source_tanks) > 0:
        entry.set_editor_property("tank_attach_z_offset", source_tanks[0].get_editor_property("tank_attach_z_offset"))
    set_class_override(entry, "tank_class_override", PUMA_CLASS)
    settings = detached_copy(obj.get_editor_property("m_aircraft_drop_settings"))
    settings.set_editor_property("tanks_with_offsets", [entry])
    settings.set_editor_property("squad_subtypes", [])
    obj.set_editor_property("m_aircraft_drop_settings", settings)
    set_ui(obj, "Puma Drop",
           "A Ju 390 transport lands at the target and unloads a Puma armoured car: a fast scout with a 50 mm gun.")
    set_balance(obj, "TierI", PUMA_COST, TIER_I_COOLDOWN)
    return obj


def mesh_extent(mesh):
    extent = mesh.get_bounds().box_extent
    return max(extent.x, extent.y, extent.z)


def author_vehicle_parts_drop(asset, templates, report):
    supply = templates.get(ga("GA_GerRadixiteMetalSupplyDrop"))
    obj = get_or_create(asset, templates, "GA_GerVehiclePartsSupplyDrop", "GA_GerRadixiteMetalSupplyDrop",
                        unreal.GA_ResourceDrop, report)
    supply_fall = supply.get_editor_property("m_resource_drop_settings").get_editor_property("fall_settings")
    crate_mesh = supply_fall.get_editor_property("falling_mesh")
    crate_size = mesh_extent(crate_mesh) * supply_fall.get_editor_property("falling_mesh_scale")
    parts_mesh = load(VEHICLE_PARTS_MESH)
    scale = round(crate_size / max(mesh_extent(parts_mesh), 1.0), 2)

    settings = detached_copy(obj.get_editor_property("m_resource_drop_settings"))
    fall = settings.get_editor_property("fall_settings")
    fall.set_editor_property("falling_mesh", parts_mesh)
    fall.set_editor_property("falling_mesh_scale", scale)
    settings.set_editor_property("fall_settings", fall)
    container = settings.get_editor_property("container_settings")
    container.set_editor_property("container_mesh", parts_mesh)
    container.set_editor_property("container_mesh_scale", scale)
    container.set_editor_property("rewards_per_container", {VP: VEHICLE_PARTS_PER_CRATE})
    container.set_editor_property("scavenge_seconds", SUPPLY_SCAVENGE_SECONDS)
    settings.set_editor_property("container_settings", container)
    obj.set_editor_property("m_resource_drop_settings", settings)
    count = settings.get_editor_property("payload_count")

    aim = detached_copy(obj.get_editor_property("m_aim_settings"))
    area = aim.get_editor_property("area_aim")
    area.set_editor_property("drop_point_mesh", parts_mesh)
    area.set_editor_property("drop_point_scale", scale)
    aim.set_editor_property("area_aim", area)
    obj.set_editor_property("m_aim_settings", aim)

    set_ui(obj, "Vehicle Parts Supply Drop",
           "A transport flies the line you draw and drops %d crates of vehicle parts on parachutes. Scavengers empty "
           "a crate in %d seconds for %d vehicle parts: %d in all. Other infantry with scavenging gear gets a third. "
           "Crates stay until emptied." % (count, int(SUPPLY_SCAVENGE_SECONDS), VEHICLE_PARTS_PER_CRATE * 3,
                                            VEHICLE_PARTS_PER_CRATE * 3 * count))
    set_balance(obj, "TierI", SUPPLY_COST, SUPPLY_COOLDOWN)
    report.append("vehicle parts drop: %d crates x %d VP, mesh scale %.2f (crate size %.0f cm)" % (
        count, VEHICLE_PARTS_PER_CRATE, scale, crate_size))
    return obj


def retier_flak_luchs_stug(templates, report):
    obj = templates.get(ga("GA_GerFlakLuchsStugDrop"))
    if obj is None:
        raise RuntimeError("GA_GerFlakLuchsStugDrop has no template")
    before = (obj.get_editor_property("m_ability_tier"), radixite_of(obj),
              obj.get_editor_property("m_ability_costs").get_editor_property("cool_down_time"))
    set_balance(obj, "TierII", FLAK_LUCHS_STUG_COST, TIER_II_COOLDOWN)
    report.append("flak luchs + stug before tier=%s radixite=%d cooldown=%s" % before)
    return obj


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save_to_disk(asset, report):
    """While another process holds the package the save fails silently (error 32): retry and prove the file changed."""
    file_path = disk_path(DA_PATH)
    for attempt in range(SAVE_RETRIES):
        before = os.path.getmtime(file_path)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if os.path.getmtime(file_path) > before:
            report.append("saved %s" % time.ctime(os.path.getmtime(file_path)))
            return True
        report.append("save did not reach disk, retry %d" % attempt)
        time.sleep(SAVE_RETRY_SECONDS)
    report.append("SAVE FAILED")
    return False


def describe(name, obj):
    costs = obj.get_editor_property("m_ability_costs")
    text = "%s %s %s tier=%s cd=%s costs=%s title=%s" % (
        name, obj.get_name(), obj.get_class().get_name(), obj.get_editor_property("m_ability_tier"),
        costs.get_editor_property("cool_down_time"),
        {str(k).split(".")[-1]: v for k, v in costs.get_editor_property("costs").get_editor_property(
            "resource_costs").items()},
        obj.get_editor_property("m_ui_settings").get_editor_property("title"))
    for prop in ("m_aim_settings", "m_flare_curtain_settings", "m_aircraft_drop_settings", "m_resource_drop_settings"):
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        text += " %s=%s" % (prop, value.export_text()[:900])
    return text


def run():
    global GA_ENUM
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    GA_ENUM = type(next(iter(templates.keys())))
    before = len(templates)
    mine = set(str(ga(name)) for name in MY_KEYS)
    others_before = {str(key): fingerprint(obj) for key, obj in templates.items() if str(key) not in mine}
    report, errors = ["%d entries before" % before], []
    authored = {}
    for name, author in (("GA_GerMassiveFlare", author_massive_flare), ("GA_GerPumaDrop", author_puma_drop),
                         ("GA_GerVehiclePartsSupplyDrop", author_vehicle_parts_drop)):
        try:
            obj = author(asset, templates, report)
            obj.set_editor_property("m_ability_type", ga(name))
            templates[ga(name)] = obj
            authored[name] = obj
        except Exception as error:
            errors.append("%s: %s" % (name, error))
    try:
        authored["GA_GerFlakLuchsStugDrop"] = retier_flak_luchs_stug(templates, report)
    except Exception as error:
        errors.append("GA_GerFlakLuchsStugDrop: %s" % error)
    others_unsaved = {str(key): fingerprint(obj) for key, obj in templates.items() if str(key) not in mine}
    changed = sorted(key for key in others_before if others_unsaved.get(key) != others_before[key])
    if changed:
        errors.append("other templates changed before the save: %s" % changed)
    if errors:
        return report + ["ERROR " + e for e in errors] + ["NOT SAVED"]
    asset.set_editor_property("m_ability_templates_by_type", templates)
    if not save_to_disk(asset, report):
        return report
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    report.append("entries before=%d after=%d" % (before, len(check)))
    others_after = {str(key): fingerprint(obj) for key, obj in check.items() if str(key) not in mine}
    changed = sorted(key for key in others_before if others_after.get(key) != others_before[key])
    missing = sorted(key for key in others_before if key not in others_after)
    report.append("VERIFY other templates unchanged: %d checked, changed=%s missing=%s" % (
        len(others_before), changed, missing))
    for name in MY_KEYS:
        obj = check.get(ga(name))
        report.append("VERIFY " + (describe(name, obj) if obj is not None else "%s MISSING after save" % name))
    return report


def write_report(lines):
    folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "SkyDropTest")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, "author_sky_drop_%s.txt" % time.strftime("%Y%m%d_%H%M%S"))
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


try:
    LINES = run()
except Exception as run_error:
    LINES = ["ERROR run failed: %s" % run_error, "NOT SAVED"]
LINES.append("report " + write_report(LINES))
for line in LINES:
    unreal.log(LOG_PREFIX + line)
