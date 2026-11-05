"""Round 3a: read-only dump of DA_GlobalAbilitySystem: class, icon, title and the unit settings of every template. No save."""
import unreal

DA = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
OUT = "D:/UE5Projects/RTS_Survival/Saved/AbilityIconsA/da_dump.txt"
PROPS = ("m_glider_drop_settings", "m_aircraft_drop_settings", "m_paratroopers_settings", "m_composite_drop_settings",
         "m_building_drop_settings", "m_aircraft_strafing_settings")

lines = []
asset = unreal.load_asset(DA)
templates = asset.get_editor_property("m_ability_templates_by_type")
icon_users = {}
for key, obj in templates.items():
    ui = obj.get_editor_property("m_ui_settings")
    icon = ui.get_editor_property("ability_icon")
    icon_name = icon.get_path_name() if icon else "None"
    icon_users.setdefault(icon_name, []).append(key.name)
    lines.append("=== %s | %s | icon=%s | title=%s" % (key.name, obj.get_class().get_name(), icon_name,
                                                        ui.get_editor_property("title")))
    for prop in PROPS:
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        lines.append("   %s: %s" % (prop, value.export_text()[:3000]))
lines.append("")
lines.append("ICON USERS")
for icon_name, users in sorted(icon_users.items()):
    lines.append("%s <- %s" % (icon_name, ", ".join(users)))
with open(OUT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines))
unreal.log("[IconsA] dump written %d templates" % len(templates))
