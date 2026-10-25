"""Phase 1 snapshot checks from EWeaponName_Uint32_Migration.md section 4.4.

Usage: python check_snapshot.py <snapshot.tsv> <inventory.txt> <report.md>
inventory.txt holds one /Game package path per line (assets whose bytes contain "EWeaponName").
"""
import collections
import csv
import re
import sys

WEAPON_BP_PATTERN = re.compile(r"(Turret|_Weapon_|EmbTurret|EIT_|BXP|Vehicle|Aircraft|_TW_|BP_TW|Train|StandaloneTurret|GunPit|Pit)", re.I)


def load_rows(path):
    with open(path, encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t", quoting=csv.QUOTE_NONE)
        return list(reader)


def is_restorable(row):
    return row["Kind"] == "Pin" and row["Direction"] == "In" and row["LinkSource"] == "" and row["CanonicalValue"] != ""


def main():
    snapshot_path, inventory_path, report_path = sys.argv[1:4]
    rows = load_rows(snapshot_path)
    inventory = {line.strip() for line in open(inventory_path, encoding="utf-8") if line.strip()}
    snapshot_packages = {row["AssetPackage"] for row in rows}

    out = []
    out.append(f"# EWeaponName snapshot check\n")
    out.append(f"- rows: {len(rows)}\n- packages in snapshot: {len(snapshot_packages)}\n- packages in grep inventory: {len(inventory)}\n")

    kinds = collections.Counter((row["Kind"], row["Direction"]) for row in rows)
    out.append("\n## Rows by kind\n")
    for (kind, direction), count in sorted(kinds.items()):
        out.append(f"- {kind} / {direction}: {count}\n")

    missing = sorted(inventory - snapshot_packages)
    out.append(f"\n## 1. Inventory assets with no snapshot row ({len(missing)})\n")
    out.extend(f"- {package}\n" for package in missing)
    extra = sorted(snapshot_packages - inventory)
    out.append(f"\n## 1b. Snapshot packages not in the grep inventory ({len(extra)})\n")
    out.extend(f"- {package}\n" for package in extra)

    restorable = collections.Counter(row["AssetPackage"] for row in rows if is_restorable(row))
    struct_pins = collections.Counter(row["AssetPackage"] for row in rows if row["Kind"] == "StructPin" and row["CanonicalValue"])
    weapon_packages = sorted(p for p in snapshot_packages | inventory if WEAPON_BP_PATTERN.search(p.rsplit("/", 1)[-1]))
    recorded = collections.Counter(r["AssetPackage"] for r in rows if r["CanonicalValue"] and r["LinkSource"] == ""
                                   and not (r["Kind"] == "Pin" and r["Direction"] == "Out"))
    no_value = sorted(p for p in snapshot_packages if recorded[p] == 0)
    out.append(f"\n## 2a. Snapshot packages with no recorded weapon value at all ({len(no_value)})\n")
    out.extend(f"- {package}\n" for package in no_value)
    no_literal = [p for p in weapon_packages if restorable[p] == 0]
    out.append(f"\n## 2b. Weapon-looking assets with no restorable pin literal ({len(no_literal)} of {len(weapon_packages)})\n")
    out.append("These hold the weapon elsewhere (CDO variable, struct pin, function library); Apply does not restore them as written.\n\n")
    for package in no_literal:
        kinds_here = collections.Counter(r["Kind"] + "/" + r["Direction"] for r in rows if r["AssetPackage"] == package)
        out.append(f"- {package}: struct-pin values {struct_pins[package]}; rows {dict(kinds_here)}\n")

    # Member variable defaults live on the CDO in UE5, so an empty description default is fine when a CDO row has the value.
    cdo_values = {(r["AssetPackage"], r["PinOrPropertyPath"]) for r in rows
                  if r["Kind"] == "Property" and r["Container"] == "CDO" and r["CanonicalValue"]}
    unresolved = [r for r in rows if r["CanonicalValue"] == "" and (
        (r["Kind"] in ("Pin", "StructPin") and r["Direction"] == "In" and r["LinkSource"] == "")
        or r["Kind"] in ("LocalVariable", "Property")
        or (r["Kind"] == "Variable" and (r["AssetPackage"], r["PinOrPropertyPath"]) not in cdo_values))]
    out.append(f"\n## 3. Unresolved values ({len(unresolved)})\n")
    for r in unresolved:
        out.append(f"- {r['AssetPackage']} | {r['Kind']} | {r['Container']} | {r['NodeTitle']} | {r['PinOrPropertyPath']} | raw='{r['RawValue']}'\n")

    stale = [r for r in rows if r["CanonicalValue"] and r["RawValue"] not in (r["CanonicalValue"], "EWeaponName::" + r["CanonicalValue"])]
    out.append(f"\n## 4. Stale or redirected names ({len(stale)})\n")
    for r in stale:
        out.append(f"- {r['AssetPackage']} | {r['PinOrPropertyPath']} | {r['RawValue']} -> {r['CanonicalValue']}\n")

    wired = [r for r in rows if r["LinkSource"]]
    out.append(f"\n## 5. Wired weapon pins (manual fix list, {len(wired)})\n")
    for r in wired:
        out.append(f"- {r['AssetPackage']} | {r['Container']} | {r['NodeTitle']}.{r['PinOrPropertyPath']} <- {r['LinkSource']}\n")

    manual_kinds = [r for r in rows if r["Kind"] in ("SwitchCase", "Variable", "LocalVariable") or r["Container"] == "StructDefaults"
                    or r["NodeClass"] in ("K2Node_EnumLiteral", "K2Node_Select", "K2Node_FunctionEntry", "K2Node_FunctionResult",
                                          "K2Node_CustomEvent", "K2Node_Tunnel", "K2Node_MacroInstance")]
    out.append(f"\n## 6. Other manual-fix rows (Switch, variables, BP function signatures, literals; {len(manual_kinds)})\n")
    for r in manual_kinds:
        out.append(f"- {r['AssetPackage']} | {r['Kind']} | {r['NodeClass']} | {r['Container']} | {r['NodeTitle']} | {r['PinOrPropertyPath']} = {r['CanonicalValue']}\n")

    with open(report_path, "w", encoding="utf-8") as handle:
        handle.writelines(out)
    print("".join(out[:12]))
    print(f"missing={len(missing)} extra={len(extra)} no_value={len(no_value)} no_literal={len(no_literal)} unresolved={len(unresolved)} stale={len(stale)} wired={len(wired)} manual={len(manual_kinds)}")


if __name__ == "__main__":
    main()
