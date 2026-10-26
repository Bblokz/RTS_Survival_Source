"""Groups Blueprint compile errors of an editor log by asset and message; W marks weapon-related ones.
Usage: group_bp_errors.py <log>"""
import collections
import re
import sys

PATTERN = re.compile(r"LogBlueprint: Error: \[AssetLog\] .*?\\Content\\(.+?)\.uasset: \[Compiler\] (.*?)( from Source:.*)?$")
WEAPON = re.compile(r"weapon|enum|name|bomb|pickup|secondary|initweapon", re.I)

counts = collections.Counter()
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if "LogInit" in line:
        continue
    match = PATTERN.search(line)
    if match:
        message = re.sub(" +", " ", match.group(2))[:170]
        counts[(match.group(1).replace(chr(92), "/"), message)] += 1

print(f"{len(counts)} distinct asset/error pairs, {sum(counts.values())} errors")
for (asset, message), count in sorted(counts.items(), key=lambda item: (not WEAPON.search(item[0][1]), item[0][0])):
    flag = "W" if WEAPON.search(message) else " "
    print(f"{count:3} {flag} {asset} :: {message}")
