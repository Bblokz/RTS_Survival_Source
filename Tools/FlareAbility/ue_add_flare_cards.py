"""Imports the flare card icons and appends the FlareLaunch entries (EAimAbilityType 5) to Buttons_DataTableb rows
IdAimAbility and IdCancelAimAbility. Idempotent. Run in the editor through the UECP bridge:
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/FlareAbility/ue_add_flare_cards.py").read())
The icons come from compose_flare_icon.py (in Saved/AgentScripts/FlareAbility)."""
import uuid
import unreal
import uecp

SRC = "D:/UE5Projects/RTS_Survival/Saved/AgentScripts/FlareAbility/"
ICON_DIR = "/Game/RTS_Survival/Textures/GameUI/ActionUI/AimAbility"
TABLE = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
KEY = 5  # EAimAbilityType::FlareLaunch
NAMESPACE = "[95B0F81A4C66E84595EDD994C8BF3E3C]"  # the namespace every existing aim card entry uses
TITLE_FIELD = "CustomTitle_2_D7EC0DB745B03FE9D33C4C8DC8B96A88"
DESCRIPTION_FIELD = "CustomDescription_4_0181776C4C2CAB270DFB008AB8C99BB5"
ICON_FIELD = "CustomIcon_7_2099C90C405BC1445669BA825C19B589"
ENTRIES = {
    "IdAimAbility": ("T_AimAbility_Flare", "<Text_Title>Parachute Flare</>",
                     "Fires a parachute flare over the targeted area from the turret's discharger. "
                     "<Text_sExp>Reveals the area</> while the flare sinks to the ground."),
    "IdCancelAimAbility": ("T_Cancel_AimAbility_Flare", "<Text_Title>Cancel flare</>",
                           "Stops movement to the flare launch position."),
}
reference = unreal.load_asset(ICON_DIR + "/T_AimAbility_MortarBarrage")


def import_icon(name):
    task = unreal.AssetImportTask()
    task.filename = SRC + name + ".png"
    task.destination_path = ICON_DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(ICON_DIR + "/" + name)
    for prop in ("compression_settings", "lod_group", "mip_gen_settings", "srgb", "never_stream", "filter"):
        texture.set_editor_property(prop, reference.get_editor_property(prop))
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    return texture


def entry_text(icon, title, description):
    texture = unreal.load_asset(ICON_DIR + "/" + icon)
    path = texture.get_path_name()
    def loc(text):
        return 'NSLOCTEXT("%s", "%s", "%s")' % (NAMESPACE, uuid.uuid4().hex.upper(), text)
    return '(%d, (%s=%s,%s=%s,%s="/Script/Engine.Texture2D\'%s\'"))' % (
        KEY, TITLE_FIELD, loc(title), DESCRIPTION_FIELD, loc(description), ICON_FIELD, path)


for icon, _, _ in ENTRIES.values():
    import_icon(icon)
rows = uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(ENTRIES))["rows"]
edits = []
for row in rows:
    mapping = row["fields"]["CustomTypeMapping"]
    if "(%d, (" % KEY in mapping:
        print(row["row_name"], "already has key", KEY)
        continue
    edits.append({"row_name": row["row_name"],
                  "fields": {"CustomTypeMapping": mapping[:-1] + "," + entry_text(*ENTRIES[row["row_name"]]) + ")"}})
if edits:
    print(uecp.data.edit_data_table_rows(table_path=TABLE, edits=edits))
for row in uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(ENTRIES))["rows"]:
    mapping = row["fields"]["CustomTypeMapping"]
    print("KEYS", row["row_name"], [k for k in range(0, 10) if "(%d, (" % k in mapping])
table = unreal.load_asset(TABLE)
print("SAVED_TABLE", unreal.EditorAssetLibrary.save_loaded_asset(table, False))
