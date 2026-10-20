"""Creates the Soviet infantry / mortar behaviour Blueprints (timed UBehaviourWeapon buffs) by duplicating
BP_Beh_RapidFire_Soviet and setting their multipliers. Multipliers scale the weapon value (0.67 cooldown = 33 % less
time between shots; 0 leaves it alone). Run through the UECP bridge's execute_python.
Docs: Source/Docs/Abilities/SovietGrenades.md"""
import unreal

TEMPLATE = "/Game/RTS_Survival/Blueprints/Behaviours/Weapon/BP_Beh_RapidFire_Soviet"
DEST = "/Game/RTS_Survival/Blueprints/Behaviours/Weapon/Soviet/"
I = unreal.BehaviourIcon
BEHAVIOURS = {
    # PTRS squads: AP bullets for rifles and anti-tank rifles; launchers (bazooka) stay as they are.
    "BP_Beh_Squad_APBullets_Soviet": dict(
        duration=15.0, icon=I.ARMOR_PIERCING_BULLETS, title="Armor-Piercing Bullets",
        text="+50% Armor-pen, +15% Damage, +20% Reload-time", max_calibre=20.0, min_calibre=0.0,
        mults=dict(ArmorPenetrationMlt=1.5, DamageMlt=1.15, ReloadSpeedMlt=1.2)),
    # PPSh and large squads: close-quarters assault, faster fire and reloads, a little less accurate.
    "BP_Beh_Squad_CloseAssault_Soviet": dict(
        duration=12.0, icon=I.VULTURE_RAPID_FIRE, title="Close Assault",
        text="-33% Cooldown between shots, -50% Reload-time, +15% Damage, -15% Accuracy", max_calibre=20.0,
        min_calibre=0.0, mults=dict(BaseCooldownMlt=0.67, ReloadSpeedMlt=0.5, DamageMlt=1.15, AccuracyMlt=0.85)),
    # Mortar teams: rapid barrage; only the mortar itself (not the crew's rifles).
    "BP_Beh_MortarBarrage_Soviet": dict(
        duration=15.0, icon=I.EXPLOSIVE_SHELLS, title="Rapid Barrage",
        text="-60% Reload-time, -50% Cooldown between shots, -20% Accuracy", max_calibre=0.0, min_calibre=50.0,
        mults=dict(ReloadSpeedMlt=0.4, BaseCooldownMlt=0.5, AccuracyMlt=0.8)),
}

for name, spec in BEHAVIOURS.items():
    path = DEST + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.duplicate_asset(TEMPLATE, path)
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    mults = unreal.BehaviourWeaponMultipliers()
    for key, value in spec["mults"].items():
        mults.set_editor_property(key, value)
    cdo.set_editor_property("BehaviourWeaponMultipliers", mults)
    cdo.set_editor_property("BehaviourWeaponAttributes", unreal.BehaviourWeaponAttributes())
    cdo.set_editor_property("BehaviourLifeTime", unreal.BehaviourLifeTime.TIMED)
    cdo.set_editor_property("M_LifeTimeDuration", spec["duration"])
    cdo.set_editor_property("BehaviourIcon", spec["icon"])
    cdo.set_editor_property("M_TitleText", spec["title"])
    cdo.set_editor_property("M_DisplayText", spec["text"])
    cdo.set_editor_property("M_BuffType", unreal.BuffDebuffType.BUFF)
    cdo.set_editor_property("MinimalWeaponCalibreToApplyTo", spec["min_calibre"])
    cdo.set_editor_property("MaximumWeaponCalibreToApplyTo", spec["max_calibre"])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    cdo = unreal.get_default_object(unreal.load_asset(path).generated_class())
    print(name, "saved", saved, cdo.get_editor_property("BehaviourWeaponMultipliers").export_text(),
          cdo.get_editor_property("M_LifeTimeDuration"), cdo.get_editor_property("BehaviourStackRule"),
          cdo.get_editor_property("BehaviourIcon"))
