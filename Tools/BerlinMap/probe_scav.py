"""Read-only: bounds and scavenge rewards of power-station pieces and scavengeable tank wrecks (-run=pythonscript)."""
import json

import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall")
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
E = "/Game/RTS_Survival/Blueprints/Environment/"
roots = [E + "ScavengeObjects/DestroyedTanks", E + "ScavengeObjects/VehicleParts", E + "ScavengeObjects/SteelTowers",
         E + "DestructableEnvActor/PostApo", E + "DestructableEnvActor/Industrial/Electricity",
         E + "DestructableEnvActor/Industrial/SteelTowers"]
out = {}
for root in roots:
    for data in registry.get_assets_by_path(root, recursive=True):
        if str(data.asset_class_path.asset_name) != "Blueprint":
            continue
        path = str(data.package_name)
        name = path.rsplit("/", 1)[1]
        if "PostApo" in root and "Pwr" not in name:
            continue
        try:
            cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
            actor = sub.spawn_actor_from_class(cls, unreal.Vector(0.0, 0.0, 5000.0))
            origin, extent = actor.get_actor_bounds(False)
            entry = {"origin": [round(origin.x), round(origin.y), round(origin.z - 5000)],
                     "extent": [round(extent.x), round(extent.y), round(extent.z)],
                     "parent": cls.get_super_class().get_name() if hasattr(cls, "get_super_class") else ""}
            text = actor.export_text() if hasattr(actor, "export_text") else ""
            for comp in actor.get_components_by_class(unreal.ActorComponent):
                cname = comp.get_class().get_name()
                if "Scav" in cname or "Resource" in cname or "Reward" in cname:
                    entry.setdefault("comps", []).append(cname)
            for prop in ("scavenge_resources", "scav_resources", "resources_to_scavenge", "scavenge_time", "m_scavenge_resources",
                         "scavengable_resources", "resource_rewards", "scav_reward"):
                try:
                    entry[prop] = str(actor.get_editor_property(prop))[:400]
                except Exception:
                    pass
            out[path] = entry
            sub.destroy_actor(actor)
        except Exception as error:
            out[path] = "ERR " + str(error)[:160]
with open("D:/UE5Projects/RTS_Survival/Saved/Berlin/logs/probe_scav.json", "w") as handle:
    json.dump(out, handle, indent=1)
