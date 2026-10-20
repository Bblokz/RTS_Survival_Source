"""Appends the Soviet card entries to Buttons_DataTableb: grenades (EGrenadeAbilityType 7-10) on IdThrowGrenade /
IdCancelThrowGrenade, behaviours (EBehaviourAbilityType 18 ArmorPiercingBullets, 21 RusCloseAssault,
22 RusMortarBarrage) on IdApplyBehaviour. Run with the UECP bridge's execute_uecp_script; it skips rows that already
have the entries. The entries (titles, descriptions with fresh NSLOCTEXT keys, icons) are staged in
Saved/sovietgrenade_cards.json; "{NL}" in a description becomes the line-break token the existing entries use."""
import json

TABLE = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
ROWS = {"IdThrowGrenade": ("throw", 7), "IdCancelThrowGrenade": ("cancel", 7), "IdApplyBehaviour": ("behaviour", 18)}
entries = json.loads(open(r"D:/UE5Projects/RTS_Survival/Saved/sovietgrenade_cards.json").read())
rows = uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(ROWS))["rows"]
edits = []
for row in rows:
    key, first_new = ROWS[row["row_name"]]
    mapping = row["fields"]["CustomTypeMapping"]
    if "(%d, (" % first_new in mapping:
        print(row["row_name"], "already has key", first_new, "- skipped")
        continue
    new = entries[key]
    if "{NL}" in new:
        # The line break exactly as the "Aim For Weak Spots" entry stores it.
        start = mapping.index("For 15 seconds:") + len("For 15 seconds:")
        separator = mapping[start:mapping.index("<Text_sExp>+60%", start)]
        new = new.replace("{NL}", separator[:len(separator) // 2])
    edits.append({"row_name": row["row_name"], "fields": {"CustomTypeMapping": mapping[:-1] + "," + new + ")"}})
if edits:
    print(uecp.data.edit_data_table_rows(table_path=TABLE, edits=edits))
check = uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(ROWS))["rows"]
for row in check:
    mapping = row["fields"]["CustomTypeMapping"]
    print(row["row_name"], [k for k in range(1, 30) if "(%d, (" % k in mapping])
