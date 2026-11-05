"""Makes every global ability template in DA_GlobalAbilitySystem cost radixite only (user decision 2026-11-05).

Each template keeps its radixite amount; every other resource (vehicle parts, metal, fuel, ammo, ...) is removed.
Composite parents (UGA_CompositeDrop) own the cost: their children are made cost-free. A template that costs other
resources but no radixite is left as it is and listed as an EXCEPTION (no amounts are invented). Edits the cost
resources only; cooldowns and every other field stay as they are.

Idempotent. When nothing needs changing the run is read-only: it lists every template's cost, asserts that no
template has a non-radixite resource and does not save (that is the verification run).

The data asset holds templates of classes the Development DLL lacks, so run it headless on the DebugGame build under
Saved/AgentLocks/da_globalabilities.lockdir (and debuggame_build.lockdir, a running Cmd locks the DLL):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Report: Saved/RadixiteCosts/radixite_costs_<time>.txt (also logged with the [RadixiteCosts] prefix).

Struct values are copied through export_text/import_text into a fresh struct per template: a struct read from
get_editor_property can alias the memory of the template it came from.
"""
import os
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG_PREFIX = "[RadixiteCosts] "
RADIXITE_IMPORT_NAME = "Resource_Radixite"
SAVE_RETRIES = 12
SAVE_RETRY_SECONDS = 10.0


def enum_name(value):
    return getattr(value, "name", None) or str(value).split(".")[-1]


def resource_map(cost_state):
    """{resource name: amount} of an FGLobalAbilityCostState, read only."""
    resources = cost_state.get_editor_property("costs").get_editor_property("resource_costs")
    return {enum_name(key): int(amount) for key, amount in resources.items()}


def is_radixite(name):
    return name.replace("_", "").upper() == "RESOURCERADIXITE"


def find_group(text, field):
    """(start, end) of 'field=(...)' in an exported struct text, balanced parentheses; None when absent."""
    start = text.find(field + "=")
    if start < 0:
        return None
    index = start + len(field) + 1
    if index >= len(text) or text[index] != "(":
        # A scalar value: up to the next comma or closing parenthesis.
        end = index
        while end < len(text) and text[end] not in ",)":
            end += 1
        return start, end
    depth = 0
    for end in range(index, len(text)):
        if text[end] == "(":
            depth += 1
        elif text[end] == ")":
            depth -= 1
            if depth == 0:
                return start, end + 1
    raise RuntimeError("unbalanced %s in %s" % (field, text))


def rebuilt_cost_state(cost_state, radixite):
    """A fresh cost state struct: the exported text of cost_state with ResourceCosts replaced by radixite only
    (radixite None or 0 = an empty cost). Everything else in the text stays verbatim."""
    text = cost_state.export_text()
    group = find_group(text, "ResourceCosts")
    new_group = "ResourceCosts=((%s, %d))" % (RADIXITE_IMPORT_NAME, radixite) if radixite else "ResourceCosts=()"
    if group is not None:
        new_text = text[:group[0]] + new_group + text[group[1]:]
    else:
        costs_group = find_group(text, "Costs")
        if costs_group is None:
            raise RuntimeError("no Costs field in %s" % text)
        inner = text[costs_group[0]:costs_group[1]]
        if inner == "Costs=()":
            replaced = "Costs=(%s)" % new_group
        else:
            replaced = inner[:-1] + "," + new_group + ")"
        new_text = text[:costs_group[0]] + replaced + text[costs_group[1]:]
    fresh = type(cost_state)()
    fresh.import_text(new_text)
    wanted = {enum_name(unreal.RTSResourceType.RESOURCE_RADIXITE): radixite} if radixite else {}
    got = resource_map(fresh)
    if got != wanted:
        raise RuntimeError("import gave %s, wanted %s (text %s)" % (got, wanted, new_text))
    for field in ("cool_down_time", "cool_down_remaining"):
        if fresh.get_editor_property(field) != cost_state.get_editor_property(field):
            raise RuntimeError("%s changed in the rebuild (text %s)" % (field, new_text))
    return fresh


def fmt(resources):
    if not resources:
        return "{}"
    return "{" + ", ".join("%s %d" % (name.replace("RESOURCE_", "").lower(), amount)
                           for name, amount in sorted(resources.items())) + "}"


def composite_children(obj):
    try:
        settings = obj.get_editor_property("m_composite_drop_settings")
    except Exception:
        return []
    children = []
    for entry in settings.get_editor_property("children"):
        child = entry.get_editor_property("ability")
        if child is not None:
            children.append(child)
    return children


def plan(templates, lines):
    """Lists every template and returns (changes, exceptions, problems): changes = [(object, label, before, radixite)]."""
    changes, exceptions, problems = [], [], []
    for key in sorted(templates.keys(), key=enum_name):
        obj = templates[key]
        label = enum_name(key)
        if obj is None:
            problems.append("%s has no template object (class unknown to this DLL?)" % label)
            continue
        before = resource_map(obj.get_editor_property("m_ability_costs"))
        others = {name: amount for name, amount in before.items() if not is_radixite(name)}
        radixite = sum(amount for name, amount in before.items() if is_radixite(name))
        cls = obj.get_class().get_name()
        if others and radixite:
            changes.append((obj, label, before, radixite))
            lines.append("TEMPLATE %-34s %-28s %s -> %s" % (label, cls, fmt(before), fmt({"RESOURCE_RADIXITE": radixite})))
        elif others:
            exceptions.append("%s %s: %s (no radixite; left as it is)" % (label, cls, fmt(before)))
            lines.append("TEMPLATE %-34s %-28s %s EXCEPTION no radixite" % (label, cls, fmt(before)))
        else:
            lines.append("TEMPLATE %-34s %-28s %s unchanged" % (label, cls, fmt(before)))
        for child in composite_children(obj):
            child_before = resource_map(child.get_editor_property("m_ability_costs"))
            child_label = "%s/%s" % (label, child.get_name())
            if child_before:
                changes.append((child, child_label, child_before, 0))
                lines.append("  CHILD %-40s %s -> {} (composite parent pays)" % (child.get_name(), fmt(child_before)))
            else:
                lines.append("  CHILD %-40s {} unchanged" % child.get_name())
    return changes, exceptions, problems


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save_to_disk(asset, lines):
    """Saves and proves the file changed: while another process holds the package the save fails (error 32), so
    retry. Never wait for the file inside this process: it holds its own loaded copy."""
    file_path = disk_path(DA_PATH)
    for attempt in range(SAVE_RETRIES):
        before = os.path.getmtime(file_path)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if os.path.getmtime(file_path) > before:
            lines.append("SAVED %s mtime %s (was %s)" % (file_path, time.ctime(os.path.getmtime(file_path)),
                                                         time.ctime(before)))
            return True
        lines.append("save did not reach disk (error 32?), retry %d" % attempt)
        time.sleep(SAVE_RETRY_SECONDS)
    lines.append("SAVE FAILED")
    return False


def non_radixite_left(templates, exception_labels):
    bad = []
    for key, obj in templates.items():
        label = enum_name(key)
        if obj is None:
            bad.append("%s: no object" % label)
            continue
        costs = resource_map(obj.get_editor_property("m_ability_costs"))
        if label not in exception_labels and any(not is_radixite(name) for name in costs):
            bad.append("%s: %s" % (label, fmt(costs)))
        for child in composite_children(obj):
            child_costs = resource_map(child.get_editor_property("m_ability_costs"))
            if child_costs:
                bad.append("%s/%s child has a cost: %s" % (label, child.get_name(), fmt(child_costs)))
    return bad


def run():
    lines = []
    asset = unreal.load_asset(DA_PATH)
    if asset is None:
        return ["RESULT FAIL: could not load " + DA_PATH]
    templates = asset.get_editor_property("m_ability_templates_by_type")
    count_before = len(templates)
    lines.append("%d templates in %s (file mtime %s)" % (count_before, DA_PATH,
                                                          time.ctime(os.path.getmtime(disk_path(DA_PATH)))))
    sample = next((obj for obj in templates.values() if obj is not None
                   and resource_map(obj.get_editor_property("m_ability_costs"))), None)
    if sample is not None:
        lines.append("export sample %s: %s" % (sample.get_name(), sample.get_editor_property("m_ability_costs")
                                               .export_text()))
    changes, exceptions, problems = plan(templates, lines)
    exception_labels = set(text.split(" ")[0] for text in exceptions)
    if problems:
        return lines + ["PROBLEM " + p for p in problems] + ["RESULT FAIL: problems found, NOT SAVED"]

    if not changes:
        bad = non_radixite_left(templates, exception_labels)
        lines.append("read-only run: nothing to change, not saved")
        lines += ["EXCEPTION " + e for e in exceptions]
        if bad:
            return lines + ["NON-RADIXITE " + b for b in bad] + ["RESULT FAIL"]
        return lines + ["RESULT PASS: %d templates, none costs anything but radixite (%d exceptions)" % (
            count_before, len(exceptions))]

    # Build every new struct first; only write when all of them imported cleanly.
    rebuilt, errors = [], []
    for obj, label, before, radixite in changes:
        try:
            rebuilt.append((obj, label, rebuilt_cost_state(obj.get_editor_property("m_ability_costs"), radixite)))
        except Exception as error:
            errors.append("%s: %s" % (label, error))
    if errors:
        return lines + ["ERROR " + e for e in errors] + ["RESULT FAIL: NOT SAVED"]
    for obj, label, cost_state in rebuilt:
        obj.set_editor_property("m_ability_costs", cost_state)
    lines.append("changed %d objects in memory" % len(rebuilt))

    # Every other template must be untouched by the writes (aliasing guard).
    after_lines = []
    plan(templates, after_lines)
    bad = non_radixite_left(templates, exception_labels)
    if bad:
        return lines + ["NON-RADIXITE after edit " + b for b in bad] + ["RESULT FAIL: NOT SAVED"]
    for obj, label, before, radixite in changes:
        got = resource_map(obj.get_editor_property("m_ability_costs"))
        wanted = {"RESOURCE_RADIXITE": radixite} if radixite else {}
        if {name.upper(): amount for name, amount in got.items()} != wanted:
            return lines + ["MISMATCH %s: %s, wanted %s" % (label, fmt(got), fmt(wanted)), "RESULT FAIL: NOT SAVED"]

    if not save_to_disk(asset, lines):
        return lines + ["RESULT FAIL: save did not reach disk"]
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    lines.append("re-check after save (in-process copy; the separate read-only run proves the file): %d templates"
                 % len(check))
    if len(check) != count_before:
        return lines + ["RESULT FAIL: template count %d -> %d" % (count_before, len(check))]
    bad = non_radixite_left(check, exception_labels)
    if bad:
        return lines + ["NON-RADIXITE after save " + b for b in bad] + ["RESULT FAIL"]
    lines += ["EXCEPTION " + e for e in exceptions]
    return lines + ["RESULT SAVED: %d objects changed, %d templates radixite-only (%d exceptions)" % (
        len(rebuilt), len(check), len(exceptions))]


def write_report(lines):
    folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                          "RadixiteCosts")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, "radixite_costs_%s.txt" % time.strftime("%Y%m%d_%H%M%S"))
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


try:
    REPORT = run()
except Exception as error:  # report anything unexpected instead of dying silently in the commandlet
    REPORT = ["RESULT FAIL: exception %r" % error]
for line in REPORT:
    unreal.log(LOG_PREFIX + line)
unreal.log(LOG_PREFIX + "report " + write_report(REPORT))
