"""Icons B headless runner (2026-11-05). Runs in a DebugGame-Cmd commandlet, never in the shared editor:
    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
        -unattended -AllowCommandletRendering -RenderOffscreen -NoLiveCoding -DisablePlugins=Fab
(round3b_run.ps1 does that). Hold Saved/AgentLocks/debuggame_build.lockdir for the whole run, and
da_globalabilities.lockdir too when "assign" is set.

What it does comes from Saved/AbilityIconsB/run_config.json:
    {"dump": true,                      write every template's key/class/title/icon to Saved/AbilityIconsB/da_dump.json
     "render": ["Flak88_Gun", ...] | "all",   round3b_render_parts.R3B_JOBS (twice: the first capture can be starved)
     "render_old": ["Ju87_Dive"],        jobs from render_ability_parts.JOBS
     "import": ["GerFlak88Drop", ...],   Saved/GlobalAbilityIcons/out/T_GA_<Name>.png -> T_GlobalAbility_<Name>
     "assign": {"GA_GerFlak88Drop": "GerFlak88Drop", ...}}   sets only M_UISettings.AbilityIcon on those keys
Idempotent: re-imports replace the same textures, assigning the same icon again changes nothing.
"""
import json
import os
import time
import traceback

import unreal

LOG = "[IconsB] "
WORK = "D:/UE5Projects/RTS_Survival/Saved/AbilityIconsB/"
CONFIG = WORK + "run_config.json"
DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
ICON_DIR = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/out"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities"
# Same import settings as import_ability_icons.py (the existing icons).
MAX_SIZE = 256
STUDIO_MAP = "/Engine/Maps/Templates/Template_Default"


def log(message):
    unreal.log(LOG + message)
    with open(WORK + "headless_log.txt", "a", encoding="utf-8") as fh:
        fh.write(time.strftime("%H:%M:%S ") + message + "\n")


def norm(name):
    return name.replace("_", "").lower()


def templates_by_key(data_asset):
    return {key.name: value for key, value in data_asset.get_editor_property("M_AbilityTemplatesByType").items()}


def dump_templates():
    data_asset = unreal.load_asset(DA_PATH)
    out = []
    for key_name, template in templates_by_key(data_asset).items():
        entry = {"key": key_name, "class": template.get_class().get_name() if template else None}
        try:
            ui = template.get_editor_property("M_UISettings")
            icon = ui.get_editor_property("AbilityIcon")
            entry["icon"] = icon.get_path_name() if icon else None
            entry["title"] = str(ui.get_editor_property("Title"))
        except Exception as error:
            entry["error"] = str(error)
        out.append(entry)
    with open(WORK + "da_dump.json", "w", encoding="utf-8") as fh:
        json.dump(out, fh, indent=1)
    log("dumped %d templates" % len(out))


def render(config):
    jobs, old = config.get("render") or [], config.get("render_old") or []
    if not jobs and not old:
        return
    unreal.EditorLoadingAndSavingUtils.load_map(STUDIO_MAP)
    unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/round3b_render_parts.py").read(),
         globals())
    names = list(R3B_JOBS) if jobs == "all" else jobs
    try:
        for attempt in range(int(config.get("passes", 2))):
            if old:
                run_jobs(old)
            r3b_run(names)
            log("render pass %d done: %s %s" % (attempt + 1, names, old))
    finally:
        unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 1")


def import_texture(name):
    asset_name = "T_GlobalAbility_" + name
    task = unreal.AssetImportTask()
    task.filename = "%s/T_GA_%s.png" % (ICON_DIR, name)
    task.destination_path = FOLDER
    task.destination_name = asset_name
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + asset_name)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("max_texture_size", MAX_SIZE)
    saved = unreal.EditorAssetLibrary.save_asset(FOLDER + "/" + asset_name, only_if_is_dirty=False)
    file_path = "D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Textures/GameUI/GlobalAbilities/%s.uasset" % asset_name
    log("imported %s saved=%s mtime=%s" % (asset_name, saved, time.ctime(os.path.getmtime(file_path))
                                             if os.path.exists(file_path) else "MISSING"))
    return texture


def fingerprint(template):
    """Everything but the icon: assign must leave the rest of each template untouched."""
    parts = []
    for prop in ("M_UISettings",):
        try:
            text = template.get_editor_property(prop).export_text()
            parts.append("".join(seg for seg in text.split(",") if "AbilityIcon" not in seg))
        except Exception:
            pass
    return "|".join(parts)


DLL = "D:/UE5Projects/RTS_Survival/Binaries/Win64/UnrealEditor-RTS_Survival-Win64-DebugGame.dll"
ENUM_HEADER = ("D:/UE5Projects/RTS_Survival/Source/RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/"
               "EGlobalAbilityType.h")


def assign(mapping):
    # A DLL older than the enum header drops the templates of keys it doesn't know when the DA is saved.
    if os.path.getmtime(DLL) < os.path.getmtime(ENUM_HEADER):
        log("ASSIGN REFUSED: DebugGame DLL (%s) is older than EGlobalAbilityType.h (%s)"
            % (time.ctime(os.path.getmtime(DLL)), time.ctime(os.path.getmtime(ENUM_HEADER))))
        return
    data_asset = unreal.load_asset(DA_PATH)
    templates = templates_by_key(data_asset)
    by_norm = {norm(k): k for k in templates}
    before_all = {k: fingerprint(t) for k, t in templates.items()}
    changed = []
    for ability, name in mapping.items():
        key = by_norm.get(norm(ability))
        if key is None:
            log("ASSIGN MISSING template %s" % ability)
            continue
        template = templates[key]
        texture = unreal.load_asset(FOLDER + "/T_GlobalAbility_" + name)
        if texture is None:
            log("ASSIGN MISSING texture %s" % name)
            continue
        old = template.get_editor_property("M_UISettings")
        current = old.get_editor_property("AbilityIcon")
        if current and current.get_path_name() == texture.get_path_name():
            log("ASSIGN same %s -> %s" % (key, name))
            continue
        # A fresh struct per template: edited copies of get_editor_property have aliased between templates.
        settings = type(old)()
        settings.import_text(old.export_text())
        settings.set_editor_property("AbilityIcon", texture)
        template.modify()
        template.set_editor_property("M_UISettings", settings)
        changed.append(key)
        log("ASSIGN %s: %s -> %s" % (key, current.get_name() if current else None, texture.get_name()))
    after_all = {k: fingerprint(t) for k, t in templates.items()}
    drift = [k for k in templates if before_all[k] != after_all[k]]
    log("fingerprint drift (must be empty): %s" % drift)
    if not changed:
        log("nothing to save")
        return
    if drift:
        log("NOT SAVING: fields other than the icon changed")
        return
    data_asset.modify()
    package_file = "D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem.uasset"
    before = os.path.getmtime(package_file)
    for attempt in range(40):
        unreal.EditorAssetLibrary.save_loaded_asset(data_asset, False)
        if os.path.getmtime(package_file) != before:
            log("DA saved (attempt %d), %d templates, changed %s" % (attempt + 1, len(templates), changed))
            break
        log("DA save did not reach disk (attempt %d), retrying" % (attempt + 1))
        time.sleep(15)
    else:
        log("DA SAVE FAILED")
    # read back from the saved object
    for key in changed:
        icon = templates[key].get_editor_property("M_UISettings").get_editor_property("AbilityIcon")
        log("READBACK %s %s" % (key, icon.get_name() if icon else None))


def main():
    os.makedirs(WORK, exist_ok=True)
    with open(CONFIG, encoding="utf-8") as fh:
        config = json.load(fh)
    log("start %s" % config)
    steps = [("dump", lambda: dump_templates() if config.get("dump") else None),
             ("render", lambda: render(config)),
             ("import", lambda: [import_texture(n) for n in config.get("import") or []]),
             ("assign", lambda: assign(config["assign"]) if config.get("assign") else None)]
    for name, step in steps:
        try:
            step()
        except Exception:
            log("STEP %s FAILED\n%s" % (name, traceback.format_exc()))
    log("done")


main()
