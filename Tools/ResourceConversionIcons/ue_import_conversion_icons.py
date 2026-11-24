"""Imports the converter card icons and maps them on the conversion rows of Buttons_DataTableb (editor python).

Textures: Saved/ResourceConversionIcons/out/*.png -> /Game/RTS_Survival/Textures/GameUI/ActionUI/, with the texture
settings of T_AB_CancelMolotov (the other composed card icons).
Data table: IdEnableResourceConversion / IdDisableResourceConversion get CustomTypeMapping entry 1
(EResourceConversionAbilityType::RadixiteToVehicleParts) with the new icon, title and description. The rows' own icon
and text stay for converters of other resources. Idempotent.
"""
import unreal

SOURCE = "D:/UE5Projects/RTS_Survival/Saved/ResourceConversionIcons/out/"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/ActionUI"
TEMPLATE = "/Game/RTS_Survival/Textures/GameUI/ActionUI/T_AB_CancelMolotov"
TABLE = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
ENABLE_ICON = "T_AB_ConvertRadixiteToVehicleParts"
DISABLE_ICON = "T_AB_CancelConvertRadixiteToVehicleParts"
RADIXITE_TO_VEHICLE_PARTS = 1
COPIED_SETTINGS = ("compression_settings", "lod_group", "mip_gen_settings", "srgb", "never_stream", "filter")


def import_texture(name):
    task = unreal.AssetImportTask()
    task.filename = SOURCE + name + ".png"
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + name)
    template = unreal.load_asset(TEMPLATE)
    if texture is None or template is None:
        raise RuntimeError("import failed for " + name)
    for prop in COPIED_SETTINGS:
        texture.set_editor_property(prop, template.get_editor_property(prop))
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


enable_texture = import_texture(ENABLE_ICON)
disable_texture = import_texture(DISABLE_ICON)
print("imported", enable_texture.get_path_name(), disable_texture.get_path_name())

# ---- CustomTypeMapping entries (same field layout as Source/Tools/FlareAbility/ue_add_flare_cards.py) ---------------
import uuid
import uecp

NAMESPACE = "[95B0F81A4C66E84595EDD994C8BF3E3C]"
TITLE_FIELD = "CustomTitle_2_D7EC0DB745B03FE9D33C4C8DC8B96A88"
DESCRIPTION_FIELD = "CustomDescription_4_0181776C4C2CAB270DFB008AB8C99BB5"
ICON_FIELD = "CustomIcon_7_2099C90C405BC1445669BA825C19B589"
ENTRIES = {
    "IdEnableResourceConversion": (
        enable_texture, "<Text_Title>Convert Radixite to Vehicle Parts</>",
        "Starts the converter: it keeps turning <Text_sExp>Radixite</> into <Text_sExp>Vehicle Parts</> "
        "while you have the Radixite."),
    "IdDisableResourceConversion": (
        disable_texture, "<Text_Title>Stop Radixite Conversion</>",
        "Stops the converter from turning Radixite into Vehicle Parts."),
}


def entry_text(texture, title, description):
    def loc(text):
        return 'NSLOCTEXT("%s", "%s", "%s")' % (NAMESPACE, uuid.uuid4().hex.upper(), text)
    return '(%d, (%s=%s,%s=%s,%s="/Script/Engine.Texture2D\'%s\'"))' % (
        RADIXITE_TO_VEHICLE_PARTS, TITLE_FIELD, loc(title), DESCRIPTION_FIELD, loc(description), ICON_FIELD,
        texture.get_path_name())


rows = uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(ENTRIES))["rows"]
edits = []
for row in rows:
    mapping = row["fields"]["CustomTypeMapping"]
    if "(%d, (" % RADIXITE_TO_VEHICLE_PARTS in mapping:
        print(row["row_name"], "already has key", RADIXITE_TO_VEHICLE_PARTS)
        continue
    entry = entry_text(*ENTRIES[row["row_name"]])
    new_mapping = "(" + entry + ")" if mapping.strip() in ("", "()") else mapping[:-1] + "," + entry + ")"
    edits.append({"row_name": row["row_name"], "fields": {"CustomTypeMapping": new_mapping}})
if edits:
    print(uecp.data.edit_data_table_rows(table_path=TABLE, edits=edits))
for row in uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(ENTRIES))["rows"]:
    print("ROW", row["row_name"], row["fields"]["Icon"], row["fields"]["CustomTypeMapping"])
table = unreal.load_asset(TABLE)
print("SAVED_TABLE", unreal.EditorAssetLibrary.save_loaded_asset(table, False))
