"""Round-5 data fixes in DA_GlobalAbilitySystem (user decisions 2026-11-05). Idempotent; lines prefixed [Round5Fix].

1. RequiredTechnology -> Tech_NONE on every template except GA_GerJu87GTankBusters (the Tuning agent's). The
   Tech_HarvesterSpeed gate on GA_3xJu87Strafing was a copy bug: it silently blocked the click without that tech, and
   every template copied from it inherited it. The unit requirement (RequiredUnit) is kept.
2. GA_2xBf110Strafing reads Bf 109 (Ger_Aircraft_BF110Airborne is a Bf 109); the enum key stays.
3. GA_GerRedSpineDrop and GA_GerRedSpinePairDrop: bSquadsWalkOutOfCargoDoor on (walkers leave the landed Ju 390 by
   its cargo door and walk out to their spot).
4. SK_JU309B_Animated gets the "CargoDoor" socket the walk-out starts from (the mesh had no door socket). Set
   ROUND5_SKIP_SOCKET = True in the globals to leave the mesh alone.

Every template is fingerprinted (requirements, UI, aircraft-drop settings and the rest of the copied properties)
before and after; anything that changed outside the planned edits stops the save. Struct values are rebuilt through
export_text/import_text so nothing aliases another template.

Needs an editor DLL with FAircraftDropGlobalAbilitySettings::bSquadsWalkOutOfCargoDoor and every class in the data
asset: run headless on the DebugGame build under the da_globalabilities lock (and the build lock):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
"""
import os
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
JU390_MESH = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU390B/Mesh/SK_JU309B_Animated"
KEEP_TECH = ("GA_GerJu87GTankBusters",)
BF109_KEY = "GA_2xBf110Strafing"
BF109_TITLE = "<Text_Title>2x Bf 109 Strafing</>"
BF109_DESCRIPTION = "Two Bf 109 fighters strafe along the marked strip."
WALK_OUT_KEYS = ("GA_GerRedSpineDrop", "GA_GerRedSpinePairDrop")
# The cargo door of the Ju 390 (Ju 290 family): the "Trapoklappe" loading ramp under the rear fuselage. The mesh
# (no collision) has its nose along +X: bounds X -646..579 (12.3 m), Y +-961 (wings), Z -33..202. The door sits on the
# centre line about a third of the way from the wing root to the tail; Z 0 (the drop projects it to the navmesh).
# Given in mesh space and converted to the root bone's space from the reference pose.
SOCKET_NAME = "CargoDoor"
DOOR_MESH_SPACE = (-250.0, 0.0, 0.0)
SKIP_SOCKET = globals().get("ROUND5_SKIP_SOCKET", False)
FINGERPRINT_PROPERTIES = ("m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings",
                          "m_ui_settings", "m_ability_marker", "m_ability_tier", "m_aircraft_drop_settings",
                          "m_aircraft_strafing_settings", "m_paratroopers_settings", "m_glider_drop_settings",
                          "m_barrage_settings", "m_ability_type")
LOG = "[Round5Fix] "
FAILURES = []


def log(text):
    print(LOG + text)


def name_of(value):
    return str(value).split(".")[-1].split(":")[0].strip(">")


def member(enum_type, name):
    wanted = name.replace("_", "").lower()
    for attr in dir(enum_type):
        if attr.replace("_", "").lower() == wanted:
            return getattr(enum_type, attr)
    raise RuntimeError("no %s member %s" % (enum_type.__name__, name))


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save(asset, asset_path, retries=30):
    """Saves and proves the file changed; retries while another process holds the package (error 32)."""
    file_path = disk_path(asset_path)
    for attempt in range(retries):
        before = os.path.getmtime(file_path)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if os.path.getmtime(file_path) > before:
            log("saved %s %s" % (asset_path, time.ctime(os.path.getmtime(file_path))))
            return True
        log("save of %s did not reach disk, retry %d" % (asset_path, attempt))
        time.sleep(10)
    FAILURES.append("save " + asset_path)
    return False


def detached_copy(value):
    text = value.export_text()
    copy = type(value)()
    copy.import_text(text)
    if copy.export_text() != text:
        FAILURES.append("round trip " + type(value).__name__)
    return copy


def fingerprint(obj):
    if obj is None or not hasattr(obj, "get_editor_property"):
        return {"": str(obj)}
    parts = {"class": obj.get_class().get_name()}
    for prop in FINGERPRINT_PROPERTIES:
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        parts[prop] = value.export_text() if hasattr(value, "export_text") else str(value)
    return parts


def clear_tech(obj, key_name, report):
    requirements = detached_copy(obj.get_editor_property("m_ability_requirements"))
    tech = requirements.get_editor_property("required_technology")
    if name_of(tech).replace("_", "").lower() == "technone":
        return False
    requirements.set_editor_property("required_technology", member(unreal.Technology, "Tech_NONE"))
    obj.set_editor_property("m_ability_requirements", requirements)
    report.append("%s: RequiredTechnology %s -> Tech_NONE (unit requirement kept: %s)" % (
        key_name, name_of(tech), requirements.get_editor_property("required_unit").export_text()))
    return True


def set_bf109_text(obj, report):
    ui = detached_copy(obj.get_editor_property("m_ui_settings"))
    old = (str(ui.get_editor_property("title")), str(ui.get_editor_property("description")))
    if old == (BF109_TITLE, BF109_DESCRIPTION):
        return False
    ui.set_editor_property("title", BF109_TITLE)
    ui.set_editor_property("description", BF109_DESCRIPTION)
    obj.set_editor_property("m_ui_settings", ui)
    report.append("%s: title/description %r -> %r" % (BF109_KEY, old, (BF109_TITLE, BF109_DESCRIPTION)))
    return True


def set_walk_out(obj, key_name, report):
    settings = detached_copy(obj.get_editor_property("m_aircraft_drop_settings"))
    if settings.get_editor_property("squads_walk_out_of_cargo_door"):
        return False
    settings.set_editor_property("squads_walk_out_of_cargo_door", True)
    obj.set_editor_property("m_aircraft_drop_settings", settings)
    report.append("%s: bSquadsWalkOutOfCargoDoor on" % key_name)
    return True


def author_data_asset():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    count_before = len(templates)
    enum = type(next(iter(templates.keys())))
    before = {name_of(k): fingerprint(v) for k, v in templates.items()}
    report = []
    expected_changes = {}

    for key, obj in templates.items():
        key_name = name_of(key)
        if obj is None:
            continue
        canonical = [n for n in KEEP_TECH if n.replace("_", "").lower() == key_name.replace("_", "").lower()]
        if canonical:
            log("kept %s RequiredTechnology %s (Tuning agent's)" % (
                key_name, name_of(obj.get_editor_property("m_ability_requirements").get_editor_property(
                    "required_technology"))))
            continue
        if clear_tech(obj, key_name, report):
            expected_changes.setdefault(key_name, set()).add("m_ability_requirements")

    bf109 = templates.get(member(enum, BF109_KEY))
    if bf109 is None:
        FAILURES.append("missing " + BF109_KEY)
    elif set_bf109_text(bf109, report):
        expected_changes.setdefault(name_of(member(enum, BF109_KEY)), set()).add("m_ui_settings")

    for key_text in WALK_OUT_KEYS:
        obj = templates.get(member(enum, key_text))
        if obj is None:
            FAILURES.append("missing " + key_text)
            continue
        if set_walk_out(obj, key_text, report):
            expected_changes.setdefault(name_of(member(enum, key_text)), set()).add("m_aircraft_drop_settings")

    asset.set_editor_property("m_ability_templates_by_type", templates)
    for line in report:
        log("CHANGE " + line)
    if len(templates) != count_before:
        FAILURES.append("entry count %d -> %d" % (count_before, len(templates)))
    after = {name_of(k): fingerprint(v) for k, v in templates.items()}
    for key_name, props in after.items():
        allowed = expected_changes.get(key_name, set())
        for prop, text in props.items():
            if before.get(key_name, {}).get(prop) != text and prop not in allowed:
                FAILURES.append("unplanned change %s.%s" % (key_name, prop))
    log("entries %d, %d changes planned" % (len(templates), len(report)))
    if FAILURES:
        log("NOT SAVING the DA: " + "; ".join(FAILURES))
        return
    if not report:
        log("DA already up to date, not saved")
        return
    save(asset, DA_PATH)
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    left = [name_of(k) for k, v in check.items() if v is not None and name_of(
        v.get_editor_property("m_ability_requirements").get_editor_property("required_technology")).replace(
        "_", "").lower() != "technone"]
    log("after reload: %d entries; templates with a technology requirement: %s" % (len(check), left))


def root_bone_transform(mesh):
    """Name and component-space reference transform of the mesh's root bone."""
    skeleton = mesh.get_editor_property("skeleton")
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    names = list(unreal.AnimPoseExtensions.get_bone_names(pose))
    for bone in names:
        transform = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
        log("bone %s ref translation=%s rotation=%s scale=%s" % (bone, transform.translation,
                                                                 transform.rotation.rotator(), transform.scale3d))
    root = names[0]
    return root, unreal.AnimPoseExtensions.get_ref_bone_pose(pose, root, unreal.AnimPoseSpaces.WORLD)


def add_cargo_door_socket():
    if SKIP_SOCKET:
        log("socket skipped")
        return
    mesh = unreal.load_asset(JU390_MESH)
    bone, root = root_bone_transform(mesh)
    location = root.inverse_transform_location(unreal.Vector(*DOOR_MESH_SPACE))
    back = root.transform_location(location)
    log("door mesh space %s -> %s space %s (back to mesh space %s)" % (DOOR_MESH_SPACE, bone, location, back))
    existing = mesh.find_socket(SOCKET_NAME)
    current = existing.get_editor_property("relative_location") if existing is not None else None
    same_place = current is not None and max(abs(current.x - location.x), abs(current.y - location.y),
                                             abs(current.z - location.z)) < 0.01
    if existing is not None and str(existing.get_editor_property("bone_name")) == str(bone) and same_place:
        log("socket %s already at %s on %s" % (SOCKET_NAME, location, bone))
        return
    ok = unreal.AircraftCannonPodsEditorLibrary.set_skeletal_mesh_socket(mesh, SOCKET_NAME, bone, location,
                                                                         unreal.Rotator(0.0, 0.0, 0.0))
    log("socket %s on %s at %s: %s" % (SOCKET_NAME, bone, location, ok))
    if not ok:
        FAILURES.append("socket not added")
        return
    save(mesh, JU390_MESH)


author_data_asset()
add_cargo_door_socket()
log("DONE OK" if not FAILURES else "DONE WITH FAILURES " + "; ".join(FAILURES))
