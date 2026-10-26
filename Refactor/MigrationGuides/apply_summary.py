"""Summarises a WeaponNameMigration Apply/Verify log. Usage: apply_summary.py <log>"""
import collections
import re
import sys

lines = [l for l in open(sys.argv[1], encoding="utf-8", errors="replace") if "LogInit" not in l]
for line in lines:
    if re.search(r"WeaponNameMigration (apply|verify):|Failure - |Success - ", line):
        print(line.strip()[30:])
for key in ["retyped variable", "retyped local", "restored property", "relinked", "removed conversion node", r"restored [^p]",
            "DRYRUN would save", "FAILED", "VERIFY MISSING", "VERIFY CHANGED"]:
    print(f"{key:26} {sum(1 for l in lines if re.search('WeaponNameMigration.*' + key, l) or (key.startswith('VERIFY') and key in l))}")
errors = collections.Counter()
for line in lines:
    match = re.search(r"LogBlueprint: Error: \[AssetLog\] .*\\([^\\]+)\.uasset: \[Compiler\] (.*?)( from Source:.*)?$", line)
    if match:
        errors[f"{match.group(1)}: {re.sub(' +', ' ', match.group(2))[:150]}"] += 1
print(f"BP compile errors: {sum(errors.values())}")
for text, count in errors.most_common(40):
    print(f"  {count:3} {text}")
for line in lines:
    if "WeaponNameMigration FAILED" in line or "VERIFY " in line:
        print("  " + line.strip()[30:][:260])
