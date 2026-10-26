"""Builds the snapshots for re-applying the EWeaponName migration after merging main (see EWeaponName_Merge_Runbook.md).

Usage:
    python -I make_delta_snapshot.py <main_snapshot.tsv> <changed_content.txt> <original_snapshot.tsv> <out_dir>

main_snapshot.tsv      Snapshot taken on main's merge commit while EWeaponName is still uint8.
changed_content.txt    `git diff --name-only --diff-filter=AMR <branch point> <main commit> -- Content` output.
original_snapshot.tsv  EWeaponName_Snapshot.tsv from the migration branch.

Writes to out_dir:
    EWeaponName_Snapshot_Delta.tsv     main's rows for packages main added or changed: the Apply input.
    EWeaponName_Snapshot_Combined.tsv  original rows for untouched packages plus the delta rows: the Verify input.
"""
import os
import sys


def read_tsv(path):
    with open(path, encoding="utf-8") as handle:
        lines = handle.read().splitlines()
    return lines[0], lines[1:]


def to_package(content_path):
    # "Content/RTS_Survival/X/Y.uasset" -> "/Game/RTS_Survival/X/Y"
    path = content_path.strip().replace("\\", "/")
    if not path.startswith("Content/") or not path.endswith((".uasset", ".umap")):
        return None
    return "/Game/" + path[len("Content/"):].rsplit(".", 1)[0]


def main():
    main_snapshot, changed_list, original_snapshot, out_dir = sys.argv[1:5]
    main_header, main_rows = read_tsv(main_snapshot)
    original_header, original_rows = read_tsv(original_snapshot)
    if main_header != original_header:
        sys.exit("Snapshot headers differ: take the main snapshot with the same commandlet version as the branch.")

    with open(changed_list, encoding="utf-8") as handle:
        changed = {package for package in map(to_package, handle) if package}

    delta_rows = [row for row in main_rows if row.split("\t", 1)[0] in changed]
    delta_packages = {row.split("\t", 1)[0] for row in delta_rows}
    # A package main changed but that no longer holds weapon data keeps no rows: main's version wins as is.
    kept_rows = [row for row in original_rows if row.split("\t", 1)[0] not in changed]

    os.makedirs(out_dir, exist_ok=True)
    for name, rows in (("EWeaponName_Snapshot_Delta.tsv", delta_rows),
                       ("EWeaponName_Snapshot_Combined.tsv", kept_rows + delta_rows)):
        with open(os.path.join(out_dir, name), "w", encoding="utf-8", newline="\n") as handle:
            handle.write("\n".join([main_header] + rows) + "\n")

    original_packages = {row.split("\t", 1)[0] for row in original_rows}
    print(f"changed Content packages on main: {len(changed)}")
    print(f"delta: {len(delta_rows)} rows in {len(delta_packages)} packages "
          f"({len(delta_packages & original_packages)} were migrated on the branch, "
          f"{len(delta_packages - original_packages)} are new to the migration)")
    print(f"combined: {len(kept_rows) + len(delta_rows)} rows")


if __name__ == "__main__":
    main()
