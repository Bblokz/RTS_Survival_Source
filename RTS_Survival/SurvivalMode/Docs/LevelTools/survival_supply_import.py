# Survival mode — (re)import Docs/supply_costs.json into DA_SurvivalMode_Default's Army settings.
# Run in an editor that has the SurvivalMode classes (execute_python). Touches only the data asset.
import json
import unreal

DOCS = "D:/UE5Projects/RTS_Survival/Source/RTS_Survival/SurvivalMode/Docs/"
SETTINGS = "/Game/RTS_Survival/SurvivalMode/Data/DA_SurvivalMode_Default"

supply = json.load(open(DOCS + "supply_costs.json", encoding="utf-8"))
entries = []
for unit in supply["units"]:
    variants = unit.get("variants") or {}
    variant_text = ",".join("(%s, %d)" % (k, v) for k, v in variants.items())
    # Super-heavies (40+ supply: King Tiger, Maus, E-100, ...) once per army so a start stays a trade-off.
    max_copies = 1 if unit["base_supply"] >= 40 else 0
    name = unit.get("display", "").replace('"', "'")
    entries.append('(Unit=(UnitType=%s,SubtypeValue=%d),DisplayName=INVTEXT("%s"),BaseSupply=%d,VariantSupply=(%s),'
                   'MaxCopies=%d)' % (unit["unit_type"], unit["subtype_value"], name, unit["base_supply"],
                                      variant_text, max_copies))
fallback = ",".join("(%s, %d)" % (k, v) for k, v in supply["fallback_per_unit_type"].items())
defaults = supply["suggested_defaults"]

settings = unreal.load_asset(SETTINGS)
army = settings.get_editor_property("army")
army.import_text(("(SlotCount=%d,SupplyBudget=%d,UnitSupplyCosts=(%s),FallbackSupplyPerUnitType=(%s),"
                  "AllowedUnitTypes=(UNType_Squad,UNType_Tank,UNType_Harvester,UNType_Nomadic))") % (
    defaults["SlotCount"], defaults["SupplyBudget"], ",".join(entries), fallback))
settings.set_editor_property("army", army)
unreal.EditorAssetLibrary.save_loaded_asset(settings, False)
costs = settings.get_editor_property("army").get_editor_property("unit_supply_costs")
print("supply entries", len(costs), "first", costs[0].get_editor_property("display_name"),
      "max copies 1:", sum(1 for c in costs if c.get_editor_property("max_copies") == 1))
