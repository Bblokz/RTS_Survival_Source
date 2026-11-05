"""Authors the Feuersturm fire strikes in DA_GlobalAbilitySystem. Touches only these two keys:
  GA_GerIncendiaryJu87  UGA_IncendiaryAirStrike (new class): a Ju 87 flies the drawn strip and drops incendiary
                        canisters; every canister leaves burning ground (URTSFireSubsystem::RequestGroundFire).
  GA_GerFireBarrage     UGA_Barrage (existing class): an artillery barrage whose shells carry FWeaponData::GroundFire,
                        so every impact leaves burning ground through the same fire subsystem.

Run headless with a DebugGame build that has UGA_IncendiaryAirStrike:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Idempotent: re-running rebuilds both templates from their sources (GA_GerAirMinefield / GA_Barrage) and these numbers.
Saving the data asset from an editor whose DLL lacks UGA_IncendiaryAirStrike drops the Ju 87 template, so never save
the DA from the shared Development editor until it is rebuilt. Design and numbers: Source/Docs/Abilities/FireStrikes.md,
test: -FireStrikesValidate.
"""
import re
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG_PREFIX = "[FireStrikes] "
SAVE_ATTEMPTS = 6
SAVE_RETRY_SECONDS = 10
ICON_DIR = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/"

JU87_AIRBORNE = ("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Ger_Aircraft_StukaJu87_Airborne."
                 "Ger_Aircraft_StukaJu87_Airborne_C")
CANISTER_MESH = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/BombMeshes/SM_Bomb_250.SM_Bomb_250"
CANISTER_IMPACT = "/Game/RTS_Survival/SpecialEffects/Molotov/NS_MolotovImpact.NS_MolotovImpact"
CANISTER_SOUND = "/Game/RTS_Survival/Sounds/Explosions/Grenades/Molotov/Cue_MolotovImpact.Cue_MolotovImpact"
STRIP_MATERIAL = "/Game/RTS_Survival/Materials/AimMaterials/TargetAreas/M_TargetArea_Minefield"
DROP_POINT_MATERIAL = "/Game/RTS_Survival/Materials/AimMaterials/TargetAreas/M_DropPointGhost"

# Rear armor up to 20 burns fully, falling off to nothing at 60: infantry, soft and light vehicles, not tanks.
# Fire also has to beat each target's fire buffer (DeveloperSettings UnitResistances): infantry recover 5/s, light
# armour 20/s, medium 25/s, heavy 35/s, so 25 dps burns infantry and buildings, barely scratches light armour and only
# fills the buffer of medium and heavy tanks.
FIRE_ARMOR = (20.0, 2.0, 60.0)

JU87 = {
    "key": "GA_GerIncendiaryJu87",
    "source": "GA_GerAirMinefield",
    "title": "Incendiary Ju 87",
    "description": ("A Ju 87 flies the line you draw and drops 6 incendiary canisters along it. Each sets the ground "
                    "ablaze for 18 seconds: deadly to infantry, harmless to tanks. If it is shot down, the rest are "
                    "lost."),
    "icon": "T_GlobalAbility_3xJu87Strafing",
    "costs": {"RESOURCE_RADIXITE": 450},  # radixite only (user decision 2026-11-05)
    "cooldown": 210,
    # (min length, max length, length before anchor, half width)
    "strip": (2000.0, 4500.0, 3000.0, 450.0),
    "canisters": 6,
    "fire": {"radius": 400.0, "duration": 18.0, "dps": 25.0, "vfx": 3.0},
    "impact_damage": 40.0,
}

BARRAGE = {
    "key": "GA_GerFireBarrage",
    "source": "GA_Barrage",
    "title": "Fire Barrage",
    "description": ("An off-map battery fires 8 incendiary shells into the target area. Every impact leaves the "
                    "ground burning for 20 seconds, driving infantry out of cover and bunkers."),
    "icon": "T_GlobalAbility_Barrage",
    "costs": {"RESOURCE_RADIXITE": 700},
    "cooldown": 240,
    "shells": 8,
    "radius": 1000.0,
    "fire": {"radius": 400.0, "duration": 20.0, "dps": 25.0, "vfx": 3.0},
    # Every surface bursts into the molotov fireball (burst, burning pool, smoke) instead of a dirt explosion;
    # at 1.8 the fireball is about as wide as the burning area (3.5 hid the soldiers under a 15 m ball).
    "impact_scale": 1.8,
    # Incendiary shells carry less explosive than the plain barrage's (250 damage, 80 g TNT, 50 shrapnel).
    "shell_damage": 160.0,
    "shell_tnt": 60.0,
    "shell_shrapnel_damage": 35.0,
}

JU87_COPIED_ON_CREATE = ("m_ability_requirements", "m_aim_settings", "m_ability_sound_settings", "m_ability_marker",
                         "m_strip_flight_settings")
BARRAGE_COPIED_ON_CREATE = ("m_ability_requirements", "m_aim_settings", "m_ability_sound_settings",
                            "m_ability_marker", "m_barrage_settings", "m_weapon_data", "m_weapon_vfx",
                            "m_projectile_mover")


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


def ground_fire_text(fire):
    full, falloff, max_pen = FIRE_ARMOR
    return ("(RadiusCm=%f,DurationSeconds=%f,DamagePerSecond=%f,DamageFalloffExponent=0.000000,FullArmorPen=%f,"
            "ArmorPenFallOff=%f,MaxArmorPen=%f,VfxScale=%f)" % (fire["radius"], fire["duration"], fire["dps"], full,
                                                                falloff, max_pen, fire["vfx"]))


def import_struct(obj, prop, text):
    """Struct fields marked EditDefaultsOnly refuse set_editor_property on instanced templates; text import works."""
    value = obj.get_editor_property(prop)
    value.import_text(text)
    obj.set_editor_property(prop, value)
    return obj.get_editor_property(prop).export_text()


def copy_from_source(obj, source, props):
    # Each value goes through text so nothing on the new template can alias the source's struct wrappers.
    for prop in props:
        import_struct(obj, prop, source.get_editor_property(prop).export_text())


def apply_ui_and_balance(obj, spec):
    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("title", "<Text_Title>%s</>" % spec["title"])
    ui.set_editor_property("description", spec["description"])
    ui.set_editor_property("ability_icon", load(ICON_DIR + spec["icon"]))
    obj.set_editor_property("m_ui_settings", ui)
    obj.set_editor_property("m_ability_tier", unreal.GlobalAbilityTier.TIER_III)
    cost_state = obj.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs",
                                  {getattr(unreal.RTSResourceType, k): v for k, v in spec["costs"].items()})
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", spec["cooldown"])
    cost_state.set_editor_property("cool_down_remaining", 0)
    obj.set_editor_property("m_ability_costs", cost_state)


def apply_ju87(obj):
    spec = JU87
    flight = obj.get_editor_property("m_strip_flight_settings")
    flight.set_editor_property("aircraft_class", unreal.load_class(None, JU87_AIRBORNE))
    obj.set_editor_property("m_strip_flight_settings", flight)

    min_length, max_length, before_anchor, half_width = spec["strip"]
    aim = obj.get_editor_property("m_aim_settings")
    area = aim.get_editor_property("area_aim")
    area.set_editor_property("shape", unreal.GlobalAbilityAimShape.STRIP)
    area.set_editor_property("area_material", load(STRIP_MATERIAL))
    area.set_editor_property("area_radius", half_width)
    area.set_editor_property("min_strip_length", min_length)
    area.set_editor_property("max_strip_length", max_length)
    area.set_editor_property("length_before_anchor", before_anchor)
    area.set_editor_property("shader_marker_count", 0)
    area.set_editor_property("drop_point_mesh", load(CANISTER_MESH.split(".")[0]))
    area.set_editor_property("drop_point_material", load(DROP_POINT_MATERIAL))
    area.set_editor_property("drop_point_scale", 3.0)
    aim.set_editor_property("area_aim", area)
    obj.set_editor_property("m_aim_settings", aim)

    settings = ("(CanisterCount=%d,SideScatterFraction=0.350000,ReleaseLeadDistance=900.000000,CanisterSettings=("
                "FallingMesh=\"%s\",MeshScale=1.200000,MeshRotationOffset=(Pitch=0.000000,Yaw=0.000000,Roll=0.000000),"
                "FallSeconds=1.600000,ImpactEffect=\"%s\",ImpactEffectScale=1.800000,ImpactSound=\"%s\","
                "ImpactDamage=%f,ImpactRadiusCm=%f,GroundFire=%s))"
                % (spec["canisters"], CANISTER_MESH, CANISTER_IMPACT, CANISTER_SOUND, spec["impact_damage"],
                   spec["fire"]["radius"], ground_fire_text(spec["fire"])))
    return import_struct(obj, "m_incendiary_settings", settings)


def replace_field(text, field, value):
    pattern = r"(?<![A-Za-z])%s=[^,()]*" % field
    if re.search(pattern, text) is None:
        raise RuntimeError("field %s not in %s" % (field, text[:200]))
    return re.sub(pattern, "%s=%s" % (field, value), text, count=1)


def closing_paren(text, open_index):
    depth = 0
    for index in range(open_index, len(text)):
        depth += {"(": 1, ")": -1}.get(text[index], 0)
        if depth == 0:
            return index
    raise RuntimeError("unbalanced parentheses")


def apply_barrage(obj):
    spec = BARRAGE
    barrage = obj.get_editor_property("m_barrage_settings").export_text()
    barrage = replace_field(barrage, "AmountShells", str(spec["shells"]))
    barrage = replace_field(barrage, "Radius", "%f" % spec["radius"])
    barrage = replace_field(barrage, "MinTimeBetweenBurst", "0.500000")
    barrage = replace_field(barrage, "MaxTimeBetweenBurst", "0.900000")
    import_struct(obj, "m_barrage_settings", barrage)

    weapon = obj.get_editor_property("m_weapon_data").export_text()
    weapon = replace_field(weapon, "BaseDamage", "%f" % spec["shell_damage"])
    weapon = replace_field(weapon, "TNTExplosiveGrams", "%f" % spec["shell_tnt"])
    weapon = replace_field(weapon, "ShrapnelDamage", "%f" % spec["shell_shrapnel_damage"])
    weapon = re.sub(r"GroundFire=\([^()]*\)", "GroundFire=" + ground_fire_text(spec["fire"]), weapon, count=1)
    weapon_after = import_struct(obj, "m_weapon_data", weapon)

    vfx = obj.get_editor_property("m_weapon_vfx").export_text()
    start = vfx.index("SurfaceImpactEffects=(")
    end = closing_paren(vfx, start + len("SurfaceImpactEffects="))
    surfaces = re.sub(r'ImpactEffect="[^"]*"', 'ImpactEffect="%s"' % CANISTER_IMPACT, vfx[start:end + 1])
    vfx = vfx[:start] + surfaces + vfx[end + 1:]
    scale = spec["impact_scale"]
    vfx = re.sub(r"ImpactScale=\([^()]*\)", "ImpactScale=(X=%f,Y=%f,Z=%f)" % (scale, scale, scale), vfx, count=1)
    import_struct(obj, "m_weapon_vfx", vfx)

    aim = obj.get_editor_property("m_aim_settings")
    # The crosshair covers the shells' radius plus the fire they leave at its edge.
    aim.set_editor_property("aim_radius", spec["radius"] + spec["fire"]["radius"])
    area = aim.get_editor_property("area_aim")
    area.set_editor_property("area_radius", spec["radius"])
    aim.set_editor_property("area_aim", area)
    obj.set_editor_property("m_aim_settings", aim)
    return weapon_after


def source_text(obj, props):
    return "|".join(obj.get_editor_property(prop).export_text() for prop in props)


def build(asset, templates, enum_type, spec, cls, copied, apply, report):
    key = member(enum_type, spec["key"])
    source = templates.get(member(enum_type, spec["source"]))
    if source is None:
        raise RuntimeError("source template %s missing" % spec["source"])
    obj = templates.get(key)
    if obj is not None and obj.get_class() != cls.static_class():
        raise RuntimeError("existing template is a %s" % obj.get_class().get_name())
    if obj is None:
        obj = unreal.new_object(cls, outer=asset, name="%s_%s" % (cls.__name__, spec["key"]))
        report.append("created %s" % spec["key"])
    # Rebuilt from the source every run, so a re-run never compounds an edit.
    copy_from_source(obj, source, copied)
    obj.set_editor_property("m_ability_type", key)
    apply_ui_and_balance(obj, spec)
    report.append("%s payload %s" % (spec["key"], apply(obj)[:900]))
    templates[key] = obj


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    # UGlobalAbility and EGlobalAbility share a python name, so the enum type comes from the map keys.
    enum_type = type(next(iter(templates.keys())))
    report, errors = ["%d entries before" % len(templates)], []
    sources_before = {name: source_text(templates.get(member(enum_type, name)), props)
                      for name, props in ((JU87["source"], JU87_COPIED_ON_CREATE),
                                          (BARRAGE["source"], BARRAGE_COPIED_ON_CREATE))
                      if templates.get(member(enum_type, name))}
    for spec, cls, copied, apply in ((JU87, unreal.GA_IncendiaryAirStrike, JU87_COPIED_ON_CREATE, apply_ju87),
                                     (BARRAGE, unreal.GA_Barrage, BARRAGE_COPIED_ON_CREATE, apply_barrage)):
        try:
            build(asset, templates, enum_type, spec, cls, copied, apply, report)
        except Exception as error:
            errors.append("%s: %s" % (spec["key"], error))
    for name, before in sources_before.items():
        props = JU87_COPIED_ON_CREATE if name == JU87["source"] else BARRAGE_COPIED_ON_CREATE
        if source_text(templates.get(member(enum_type, name)), props) != before:
            errors.append("source template %s changed while authoring" % name)
    if errors:
        return report + ["ERROR " + e for e in errors] + ["NOT SAVED"]
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = False
    for attempt in range(SAVE_ATTEMPTS):
        # Another process reading the package (a -game run just exiting) fails the save with error 32; retry.
        saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if saved:
            break
        time.sleep(SAVE_RETRY_SECONDS)
    report.append("saved=%s attempts=%d entries=%d" % (saved, attempt + 1, len(templates)))
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    for spec in (JU87, BARRAGE):
        obj = check.get(member(enum_type, spec["key"]))
        if obj is None:
            report.append("VERIFY %s missing after save" % spec["key"])
            continue
        report.append("VERIFY %s class=%s tier=%s costs=%s ui=%s" % (
            spec["key"], obj.get_class().get_name(), obj.get_editor_property("m_ability_tier"),
            obj.get_editor_property("m_ability_costs").export_text(),
            obj.get_editor_property("m_ui_settings").export_text()))
    return report


for line in run():
    unreal.log(LOG_PREFIX + line)
