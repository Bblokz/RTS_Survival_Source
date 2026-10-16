"""Rewrites the grenade card descriptions in Buttons_DataTableb (IdThrowGrenade CustomTypeMapping) for the
2026-10-16 grenade tuning, through the UECP bridge client (scratchpad uecp.py). Only the source string inside each
existing NSLOCTEXT changes, so the localization keys stay.
Usage: python update_grenade_card_texts.py <path to uecp.py dir>"""
import json
import re
import subprocess
import sys

UECP_DIR = sys.argv[1]
TABLE = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
DESC_FIELD = "CustomDescription_4_0181776C4C2CAB270DFB008AB8C99BB5"
# EGrenadeAbilityType value -> new description.
DESCRIPTIONS = {
    1: "Orders 2 squad members to throw HEAT grenades that stick to vehicles and explode after "
       "<Text_Exp>1 second</>, dealing <Text_Exp>300 Damage</> against the rear armor in <Text_Exp>125 Radius</>.",
    2: "Orders 3 squad members to throw a stick grenade that explodes <Text_Exp>1.5 seconds</> after landing, "
       "dealing <Text_Exp>90 Damage</> in <Text_Exp>250 Radius</>.",
    3: "Orders the squad to throw several grenades that explode <Text_Exp>1.5 seconds</> after landing, dealing "
       "<Text_Exp>60 Damage</> in <Text_Exp>200 Radius</>.",
}


def bridge(action, params):
    out = subprocess.run([sys.executable, "uecp.py", "--action", action, json.dumps(params)], cwd=UECP_DIR,
                         capture_output=True, text=True, encoding="utf-8").stdout
    return json.loads(out)


row = bridge("get_data_table_rows", {"table_path": TABLE, "row_names": ["IdThrowGrenade"]})["rows"][0]
mapping = row["fields"]["CustomTypeMapping"]
for key, text in DESCRIPTIONS.items():
    # (<key>, (... CustomDescription=NSLOCTEXT("ns", "id", "<old>") ...
    pattern = re.compile(r'(\(%d, \(.*?%s=NSLOCTEXT\("[^"]*", "[^"]*", ")(.*?)("\))' % (key, DESC_FIELD))
    mapping, count = pattern.subn(lambda m: m.group(1) + text.replace('"', '\\"') + m.group(3), mapping, count=1)
    print("key", key, "replaced" if count else "NOT FOUND")
print(json.dumps(bridge("edit_data_table_rows", {"table_path": TABLE, "edits": [
    {"row_name": "IdThrowGrenade", "fields": {"CustomTypeMapping": mapping}}]})))
check = bridge("get_data_table_rows", {"table_path": TABLE, "row_names": ["IdThrowGrenade"]})["rows"][0]
for key, text in DESCRIPTIONS.items():
    print("key", key, "ok" if text in check["fields"]["CustomTypeMapping"] else "MISSING")
