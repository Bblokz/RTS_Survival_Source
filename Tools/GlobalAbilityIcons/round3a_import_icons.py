"""Round 3a (Icons A agent, 2026-11-05): imports the composed unit-drop icons as UI textures and sets them as the
Icon of their DA_GlobalAbilitySystem templates. Touches ONLY the keys in ICONS and ONLY m_ui_settings.ability_icon.

Headless on the DebugGame build (the DA holds templates of classes the shared editor may not know), holding BOTH
Saved/AgentLocks/debuggame_build.lockdir and Saved/AgentLocks/da_globalabilities.lockdir:
    powershell -File round3a_run_import.ps1
Idempotent: re-running re-imports over the same textures. Keys missing from the DA (templates not authored yet) and
icons without a composed PNG are skipped and reported. Every other template is fingerprinted before and after.
"""
import os
import time

import unreal

ICON_DIR = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/out"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities"
DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG = "[IconsAImport] "
MAX_SIZE = 256  # as import_ability_icons.py: the button is 96 slate units, 256 stays sharp at 4K UI scale

# EGlobalAbility python name -> composed icon name (T_GA_<name>.png -> T_GlobalAbility_<name>)
ICONS = {
    "GA_GER_MARDER_SCAVENGER_DROP": "GerMarderScavengerDrop",
    "GA_GER_ELITE_SCAVENGERS_PAK_DROP": "GerEliteScavengersPakDrop",
    "GA_GER_STURM_PIONIEREN_PAK_PIT_DROP": "GerSturmPionierenPakPitDrop",
    "GA_GER_ELITE_SCAVENGERS_PARADROP": "GerEliteScavengersParadrop",
    "GA_GER_E50_DROP": "GerE50Drop",
    "GA_GER_E75_DROP": "GerE75Drop",
    "GA_GER_MAUS_DROP": "GerMausDrop",
    "GA_GER_JAGD_TIGER_LASER_DROP": "GerJagdTigerLaserDrop",
    "GA_GER_JAGD_PANTHER_FLAMER_DROP": "GerJagdPantherFlamerDrop",
    "GA_GER_RADIXITE_PZ_II_DROP": "GerRadixitePzIIDrop",
    "GA_GER_RADIXITE_PZ_III_WESPE_DROP": "GerRadixitePzIIIWespeDrop",
    "GA_GER_RAIL_TROOPERS_PARADROP": "GerRailTroopersParadrop",
    "GA_GER_RAIL_TROOPERS_DROP": "GerRailTroopersDrop",
    "GA_GER_LIGHT_BRINGERS_PARADROP": "GerLightBringersParadrop",
    "GA_GER_PZ_JAGER_LASER_DROP": "GerPzJagerLaserDrop",
    "GA_GER_FEUER_STURM_PARADROP": "GerFeuerSturmParadrop",
    "GA_GER_PZ_III_FLAMM_DROP": "GerPzIIIFlammDrop",
    "GA_GER_RED_SPINE_DROP": "GerRedSpineDrop",
    "GA_GER_RED_SPINE_PAIR_DROP": "GerRedSpinePairDrop",
    "GA_GER_PUMA_DROP": "GerPumaDrop",
    "GA_GER_PARATROOPERS": "GerParatroopers",
    "GA_GER_STURMTIGER_DROP": "GerSturmtigerDrop",
    "GA_GER_JAGD_TIGER_DROP": "GerJagdTigerDrop",
    "GA_GER_KING_TIGER105_DROP": "GerKingTiger105Drop",
    "GA_GER_TIGER_RAIL_DROP": "GerTigerRailDrop",
    "GA_GER_IRON_STORM_PARADROP": "GerIronStormParadrop",
    "GA_GER_PZ_GREN_IRON_STORM_PARADROP": "GerPzGrenIronStormParadrop",
    "GA_GER_FELS_KRIEGER_DROP": "GerFelsKriegerDrop",
    # follow-up 2026-11-05: the older drops that shared T_GlobalAbility_GerSpotterPzIIIDrop (Spotter keeps it)
    "GA_HARVESTER_DROP": "GerHarvesterDrop",
    "GA_GER_PZ_I_HARVESTER_DROP": "GerPzIHarvesterDrop",
    "GA_GER_PZ38T_RAILGUN_DROP": "GerPz38tRailgunDrop",
    "GA_SCAVENGERS_DROP": "GerScavengersDrop",
}
# Optional subset: ICONSA_KEYS="GA_GER_E50_DROP,GA_GER_MAUS_DROP"
SUBSET = [k.strip() for k in os.environ.get("ICONSA_KEYS", "").split(",") if k.strip()]


def content_file(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def import_texture(name, report):
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
    if texture is None:
        raise RuntimeError("import of %s failed" % asset_name)
    # Same settings as every other global ability icon (import_ability_icons.py).
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("max_texture_size", MAX_SIZE)
    file_path = content_file(FOLDER + "/" + asset_name)
    before = os.path.getmtime(file_path) if os.path.exists(file_path) else 0
    for attempt in range(6):
        unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
        if os.path.exists(file_path) and os.path.getmtime(file_path) > before:
            report.append("texture %s saved" % asset_name)
            return texture
        time.sleep(5)
    raise RuntimeError("texture %s did not reach disk" % asset_name)


def fingerprint(obj):
    parts = []
    for prop in ("m_ui_settings", "m_ability_costs", "m_ability_tier", "m_aim_settings"):
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        parts.append(value.export_text() if hasattr(value, "export_text") else str(value))
    return obj.get_class().get_name() + "|" + "|".join(parts)


def set_icon(obj, texture):
    old = obj.get_editor_property("m_ui_settings")
    text = old.export_text()
    settings = type(old)()
    settings.import_text(text)
    if settings.export_text() != text:
        raise RuntimeError("m_ui_settings did not round-trip through text")
    settings.set_editor_property("ability_icon", texture)
    obj.modify()
    obj.set_editor_property("m_ui_settings", settings)


def save_da(asset, report, retries=12):
    file_path = content_file(DA_PATH)
    for attempt in range(retries):
        before = os.path.getmtime(file_path)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if os.path.getmtime(file_path) > before:
            report.append("DA saved %s" % time.ctime(os.path.getmtime(file_path)))
            return True
        report.append("DA save did not reach disk (error 32?), retry %d" % attempt)
        time.sleep(10)
    report.append("DA SAVE FAILED")
    return False


def run():
    report, errors = [], []
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    by_name = {key.name: (key, obj) for key, obj in templates.items()}
    report.append("%d entries" % len(by_name))
    wanted = [k for k in ICONS if not SUBSET or k in SUBSET]
    others_before = {name: fingerprint(obj) for name, (_, obj) in by_name.items() if name not in wanted}
    done = []
    for key_name in wanted:
        name = ICONS[key_name]
        if key_name not in by_name:
            report.append("SKIP %s: no template in the DA yet" % key_name)
            continue
        if not os.path.exists("%s/T_GA_%s.png" % (ICON_DIR, name)):
            report.append("SKIP %s: no composed icon T_GA_%s.png" % (key_name, name))
            continue
        try:
            texture = import_texture(name, report)
        except Exception as error:
            # a texture another process holds open (error 32): leave this template's icon as it is
            report.append("SKIP %s: %s" % (key_name, error))
            continue
        try:
            set_icon(by_name[key_name][1], texture)
            done.append(key_name)
        except Exception as error:
            errors.append("%s: %s" % (key_name, error))
    if errors:
        report += ["ERROR " + e for e in errors] + ["DA NOT SAVED"]
        return report
    if not done:
        report.append("nothing to assign")
        return report
    asset.modify()
    if not save_da(asset, report):
        return report
    check = {key.name: obj for key, obj in unreal.load_asset(DA_PATH).get_editor_property(
        "m_ability_templates_by_type").items()}
    report.append("entries after %d" % len(check))
    changed = [n for n, fp in others_before.items() if n not in check or fingerprint(check[n]) != fp]
    report.append("OTHER TEMPLATES CHANGED: %s" % (changed or "none"))
    for key_name in done:
        icon = check[key_name].get_editor_property("m_ui_settings").get_editor_property("ability_icon")
        report.append("ICON %s -> %s" % (key_name, icon.get_name() if icon else None))
    return report


for line in run():
    unreal.log(LOG + line)
