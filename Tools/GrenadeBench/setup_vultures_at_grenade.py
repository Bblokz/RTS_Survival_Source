"""Gives the Hazmat Vultures (FG-42 squad) their AT stick grenade: all four soldiers throw an M24 stick grenade that
sticks to vehicles (EGrenadeAbilityType::StickGrenadeVultures, card key 5). Adds a GrenadeComponent to the squad
Blueprint (or updates the one it has), with the Faust HEAT grenade's sound attenuation/concurrency, effect scale,
montage time and damage type. Run through the UECP bridge's execute_python.
Docs: Source/Docs/Abilities/GrenadeQuality.md (Vultures AT stick grenade)."""
import unreal

SQUADS = "/Game/RTS_Survival/Blueprints/Squads/Ger/"
VULTURES = SQUADS + "ArmoryUnlocked/Squad_Ger_HazmatVultures"
AT_TEMPLATE = SQUADS + "Barracks_Infantry/SteelFistAssaultSquad/Squad_Ger_Faust"

# Balanced per volley: 4 x 160 = 640 against the Faust pair's 2 x 300 = 600, on a longer cooldown.
SETTINGS = dict(
    GrenadeAbility=unreal.GrenadeAbilityType.STICK_GRENADE_VULTURES,
    GrenadeMesh="/Game/RTS_Survival/Meshes/Weapons/Grenades/SM_M24StickGrenade",
    ExplosionEffect="/Game/RTS_Survival/ImpactSfxVfx/2MediumCalibre/Air_Default/NS_MediumCaliber_HighExplosive_DefaultImpact",
    ExplosionSound="/Game/RTS_Survival/Sounds/Explosions/Grenades/LargeGrenade/Cue_Grenade_large",
    ThrowRange=800.0, AoeRange=125.0, Damage=160.0, AoeDamageExponentReduction=1.5,
    FullArmorPen=75.0, ArmorPenFallOff=5.0, MaxArmorPen=125.0,
    FuseSecondsAfterLanding=1.0, bStickToVehicles=True, Cooldown=20.0,
    SquadUnitsThrowing=4, GrenadesPerSquad=1,
    # Card slot 6 holds the Vultures' rapid fire.
    PreferredIndex=7,
)
COPIED_FROM_TEMPLATE = ["ExplosionSoundAttenuation", "ExplosionSoundConcurrency", "ExplosionEffectScale",
                        "GrenadeThrowMontageDuration", "DamageType"]

sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary


def handles(bp):
    return sub.k2_gather_subobject_data_for_blueprint(bp)


def grenade_component(bp):
    for handle in handles(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj and obj.get_class().get_name() == "GrenadeComponent":
            return obj
    return None


def add_grenade_component(bp, name):
    params = unreal.AddNewSubobjectParams(parent_handle=handles(bp)[0], new_class=unreal.GrenadeComponent,
                                          blueprint_context=bp)
    handle, fail = sub.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError("add GrenadeComponent to %s failed: %s" % (bp.get_name(), fail))
    sub.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


template = grenade_component(unreal.load_asset(AT_TEMPLATE)).get_editor_property("M_Settings")
template_values = {k: template.get_editor_property(k) for k in COPIED_FROM_TEMPLATE}

bp = unreal.load_asset(VULTURES)
comp = grenade_component(bp) or add_grenade_component(bp, "ATStickGrenade")
settings = comp.get_editor_property("M_Settings")
for key, value in template_values.items():
    settings.set_editor_property(key, value)
for key, value in SETTINGS.items():
    if key in ("GrenadeMesh", "ExplosionEffect", "ExplosionSound"):
        value = unreal.load_asset(value)
        if value is None:
            raise RuntimeError("missing asset for " + key)
    settings.set_editor_property(key, value)
comp.modify()
comp.set_editor_property("M_Settings", settings)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
saved = unreal.EditorAssetLibrary.save_asset(VULTURES, only_if_is_dirty=False)
readback = grenade_component(unreal.load_asset(VULTURES)).get_editor_property("M_Settings")
print("Vultures", comp.get_name(), "saved=%s" % saved,
      {k: readback.get_editor_property(k) for k in list(SETTINGS) + COPIED_FROM_TEMPLATE})
