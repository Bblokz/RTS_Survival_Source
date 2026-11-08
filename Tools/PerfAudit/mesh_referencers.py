"""Logs, for each mesh package, the packages that reference it (asset registry, no loading).
Usage: mesh_referencers.py <pkg> [...]  -> Saved/PerfAudit/mesh_referencers.json"""
import json, os, sys
import unreal
reg = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
out = {}
args = sys.argv[1:]
if len(args) == 1 and args[0].startswith("@"):
    args = open(args[0][1:]).read().split()
for pkg in args:
    refs = reg.get_referencers(pkg, opts) or []
    out[pkg] = [str(r) for r in refs]
path = unreal.Paths.project_saved_dir() + "PerfAudit/mesh_referencers.json"
with open(path, "w") as fh:
    json.dump(out, fh, indent=1)
unreal.log_warning("RTS_REFS wrote %d" % len(out))
