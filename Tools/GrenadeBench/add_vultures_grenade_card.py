"""Adds the Vultures' AT stick grenade card (EGrenadeAbilityType::StickGrenadeVultures = key 5) to the IdThrowGrenade
and IdCancelThrowGrenade rows of Buttons_DataTableb. Run with the UECP bridge's execute_uecp_script; keys that are
already present are left alone. The numbers match setup_vultures_at_grenade.py."""
import uuid

TABLE = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
KEY = 5
NAMESPACE = "[95B0F81A4C66E84595EDD994C8BF3E3C]"
TITLE = "CustomTitle_2_D7EC0DB745B03FE9D33C4C8DC8B96A88"
DESC = "CustomDescription_4_0181776C4C2CAB270DFB008AB8C99BB5"
ICON = "CustomIcon_7_2099C90C405BC1445669BA825C19B589"
ICONS = "/Script/Engine.Texture2D'/Game/RTS_Survival/Textures/GameUI/ActionUI/%s.%s'"
CARDS = {
    "IdThrowGrenade": ("<Text_Title>Throw AT Stick Grenades</>",
                       "Orders all 4 squad members to throw M24 stick grenades that stick to vehicles and explode after "
                       "<Text_Exp>1 second</>, each dealing <Text_Exp>160 Damage</> against the rear armor in "
                       "<Text_Exp>125 Radius</>.",
                       "T_AB_ATStickGrenade"),
    "IdCancelThrowGrenade": ("<Text_Title>Cancel AT Stick Grenades</>", "Cancels the throw.",
                             "T_AB_CancelATStickGrenade"),
}


def loc(text):
    return 'NSLOCTEXT("%s", "%s", "%s")' % (NAMESPACE, uuid.uuid4().hex.upper(), text)


rows = uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(CARDS))["rows"]
edits = []
for row in rows:
    mapping = row["fields"]["CustomTypeMapping"]
    if "(%d, (" % KEY in mapping:
        print(row["row_name"], "already has key", KEY)
        continue
    title, desc, icon = CARDS[row["row_name"]]
    entry = '(%d, (%s=%s,%s=%s,%s="%s"))' % (KEY, TITLE, loc(title), DESC, loc(desc), ICON, ICONS % (icon, icon))
    edits.append({"row_name": row["row_name"], "fields": {"CustomTypeMapping": mapping[:-1] + "," + entry + ")"}})
if edits:
    print(uecp.data.edit_data_table_rows(table_path=TABLE, edits=edits))
for row in uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(CARDS))["rows"]:
    mapping = row["fields"]["CustomTypeMapping"]
    print(row["row_name"], [k for k in range(1, 30) if "(%d, (" % k in mapping])
