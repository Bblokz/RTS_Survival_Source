"""Reads a test log of ambient_headless_test.py: prints result.json's numbers and the "copies" check.

    python ambient_test_report.py Saved/Berlin/ambient/test/run6.log

The copies check: 40 copies of one curtain mesh stand alone in the view and are shown and hidden in turn; after each
change r.MeshDrawCommands.LogDynamicInstancingStats logs, per render pass, the mesh draw commands and the draw calls left
after dynamic instancing ("state buckets"). The test map renders more than one view per frame and the command logs
whichever comes next, so every block logged after a marker is listed: the main view is the one that gains 40 commands.
"""
import json
import re
import sys


def main(path):
    samples, pass_name = [], None
    for line in open(path, "r", encoding="utf-8", errors="ignore"):
        marker = re.search(r"AMBTEST COPIES (\w+)", line)
        if marker:
            samples.append((marker.group(1), []))
            continue
        if not samples:
            continue
        found = re.search(r"Instancing stats for (.+)$", line.strip())
        if found:
            pass_name = found.group(1)
            continue
        found = re.search(r"(\d+) Mesh Draw Commands in (\d+) instancing state buckets", line)
        if found and pass_name:
            samples[-1][1].append((pass_name, int(found.group(1)), int(found.group(2))))
            pass_name = None
    print("copies check: (mesh draw commands, draw calls after instancing) per logged pass block")
    for name, blocks in samples:
        for pass_key in ("DepthPass", "BasePass", "Velocity"):
            print("  %-7s %-10s %s" % (name, pass_key, [(c, b) for p, c, b in blocks if p == pass_key]))
    try:
        result = json.load(open(path.rsplit("/", 1)[0] + "/result.json"))["result"]
        for key, value in result.items():
            if key == "slice" and value:
                value = {k: v for k, v in value.items() if k != "component_names_after_replacing"}
            if key != "bench":
                print(key, "=", json.dumps(value))
    except Exception as error:  # noqa: BLE001
        print("no result.json:", error)


if __name__ == "__main__":
    main(sys.argv[1].replace("\\", "/"))
