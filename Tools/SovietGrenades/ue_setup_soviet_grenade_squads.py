"""Gives the Soviet squads their WW2 grenade abilities: adds a GrenadeComponent to each squad Blueprint (or updates the
one it already has) with the settings below. Needs an editor built with EGrenadeAbilityType::RusRGD33..RusRPG43.
Run through the UECP bridge's execute_python. Docs: Source/Docs/Abilities/SovietGrenades.md"""
import unreal

SQUADS = "/Game/RTS_Survival/Blueprints/Squads/"
MESHES = "/Game/RTS_Survival/Meshes/Weapons/Grenades/Soviet/"
GER = SQUADS + "Ger/"
FX_FRAG = "/Game/RTS_Survival/ImpactSfxVfx/1SmallCalibre/Flesh/NS_SmallCaliber_AirExplosion_1"
FX_HE = "/Game/RTS_Survival/ImpactSfxVfx/2MediumCalibre/Air_Default/NS_MediumCaliber_HighExplosive_DefaultImpact"
SND_SMALL = "/Game/RTS_Survival/Sounds/Explosions/Grenades/SmallGrenade/Cue_Grenade_small"
SND_LARGE = "/Game/RTS_Survival/Sounds/Explosions/Grenades/LargeGrenade/Cue_Grenade_large"
# The German stick (frag) and HEAT (AT) components lend attenuation, concurrency, effect scale and montage time.
FRAG_TEMPLATE = GER + "Barracks_Infantry/JagerTrupp/Squad_Ger_JagerTrupp"
AT_TEMPLATE = GER + "Barracks_Infantry/SteelFistAssaultSquad/Squad_Ger_Faust"

GRENADES = {
    "RGD33": dict(GrenadeAbility="RUS_RGD33", GrenadeMesh=MESHES + "SM_Rus_RGD33", ExplosionEffect=FX_FRAG,
                  ExplosionSound=SND_SMALL, template=FRAG_TEMPLATE,
                  ThrowRange=1000.0, AoeRange=250.0, Damage=90.0, AoeDamageExponentReduction=1.0,
                  FuseSecondsAfterLanding=1.5, bStickToVehicles=False, Cooldown=15.0, SquadUnitsThrowing=3,
                  GrenadesPerSquad=1, FullArmorPen=15.0, ArmorPenFallOff=5.0, MaxArmorPen=25.0),
    "F1": dict(GrenadeAbility="RUS_F1", GrenadeMesh=MESHES + "SM_Rus_F1", ExplosionEffect=FX_HE,
               ExplosionSound=SND_LARGE, template=FRAG_TEMPLATE,
               ThrowRange=900.0, AoeRange=350.0, Damage=65.0, AoeDamageExponentReduction=1.0,
               FuseSecondsAfterLanding=2.0, bStickToVehicles=False, Cooldown=20.0, SquadUnitsThrowing=3,
               GrenadesPerSquad=1, FullArmorPen=10.0, ArmorPenFallOff=5.0, MaxArmorPen=20.0),
    "RG42": dict(GrenadeAbility="RUS_RG42", GrenadeMesh=MESHES + "SM_Rus_RG42", ExplosionEffect=FX_FRAG,
                 ExplosionSound=SND_SMALL, template=FRAG_TEMPLATE,
                 ThrowRange=1000.0, AoeRange=200.0, Damage=55.0, AoeDamageExponentReduction=1.0,
                 FuseSecondsAfterLanding=1.0, bStickToVehicles=False, Cooldown=15.0, SquadUnitsThrowing=2,
                 GrenadesPerSquad=2, FullArmorPen=10.0, ArmorPenFallOff=5.0, MaxArmorPen=19.0),
    # The Mosin squad's molotov, copied as a whole (same card text, so the same numbers).
    "Molotov": dict(copy_all_from=SQUADS + "Rus/Rus_Squad_Mosin"),
    "RPG43": dict(GrenadeAbility="RUS_RPG43", GrenadeMesh=MESHES + "SM_Rus_RPG43", ExplosionEffect=FX_HE,
                  ExplosionSound=SND_LARGE, template=AT_TEMPLATE,
                  ThrowRange=700.0, AoeRange=125.0, Damage=300.0, AoeDamageExponentReduction=1.5,
                  FuseSecondsAfterLanding=0.75, bStickToVehicles=True, Cooldown=20.0, SquadUnitsThrowing=2,
                  GrenadesPerSquad=1, FullArmorPen=75.0, ArmorPenFallOff=5.0, MaxArmorPen=125.0),
}
# Squad Blueprint -> grenade. One grenade component per squad (ASquadController keeps a single one).
ASSIGNMENTS = {
    SQUADS + "Rus/T2/Rus_T2Squad_RedHammer": "RGD33",        # PPSh assault squad
    SQUADS + "Rus/T2/Rus_T2Squad_RedHammerPTRS": "RPG43",    # PPSh assault squad with PTRS
    SQUADS + "Rus/Rus_Squad_LargeMixBasic": "F1",            # large mixed squad
    SQUADS + "Rus/Rus_Squad_LargePTRSSquad": "RPG43",
    SQUADS + "Rus/Rus_Squad_SVT": "RG42",
    SQUADS + "Rus/Rus_Squad_MosinPTRS": "RG42",
    SQUADS + "Rus/T2/Rus_T2Squad_ToxicGuard": "Molotov",   # hazmat flamethrower squad
}
# Set ONLY = ["Molotov", ...] before exec to run a subset (the Molotov works on an editor without the new enum values).
ONLY = globals().get("ONLY")
SETTINGS_FIELDS = ["ThrowRange", "AoeRange", "Damage", "FullArmorPen", "ArmorPenFallOff", "MaxArmorPen", "DamageType",
                   "AoeDamageExponentReduction", "FuseSecondsAfterLanding", "bStickToVehicles", "Cooldown",
                   "SquadUnitsThrowing", "GrenadesPerSquad", "GrenadeThrowMontageDuration", "GrenadeMesh",
                   "ExplosionSound", "ExplosionSoundAttenuation", "ExplosionSoundConcurrency", "ExplosionEffect",
                   "ExplosionEffectScale", "AttachedGrenadeEffect", "AttachedGrenadeEffectScale", "BurnArea",
                   "GrenadeAbility", "PreferredIndex"]
COPIED_FROM_TEMPLATE = ["ExplosionSoundAttenuation", "ExplosionSoundConcurrency", "ExplosionEffectScale",
                        "GrenadeThrowMontageDuration", "DamageType"]
CARD_SLOT = 7

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


templates = {}
for path in (FRAG_TEMPLATE, AT_TEMPLATE):
    # Copy the numbers out at once: structs from get_editor_property alias the template's property.
    s = grenade_component(unreal.load_asset(path)).get_editor_property("M_Settings")
    templates[path] = {k: s.get_editor_property(k) for k in COPIED_FROM_TEMPLATE}

for path, grenade in ASSIGNMENTS.items():
    if ONLY and grenade not in ONLY:
        continue
    spec = dict(GRENADES[grenade])
    bp = unreal.load_asset(path)
    comp = grenade_component(bp) or add_grenade_component(bp, grenade + "Grenade")
    # A free-standing struct refuses EditDefaultsOnly fields; edit the component template's own settings.
    settings = comp.get_editor_property("M_Settings")
    if "copy_all_from" in spec:
        source = grenade_component(unreal.load_asset(spec["copy_all_from"])).get_editor_property("M_Settings")
        values = {key: source.get_editor_property(key) for key in SETTINGS_FIELDS}
        for key, value in values.items():
            settings.set_editor_property(key, value)
    else:
        for key, value in templates[spec.pop("template")].items():
            settings.set_editor_property(key, value)
        for key in ("GrenadeMesh", "ExplosionEffect", "ExplosionSound"):
            spec[key] = unreal.load_asset(spec[key])
        spec["GrenadeAbility"] = getattr(unreal.GrenadeAbilityType, spec["GrenadeAbility"])
        for key, value in spec.items():
            settings.set_editor_property(key, value)
        settings.set_editor_property("PreferredIndex", CARD_SLOT)
    comp.modify()
    comp.set_editor_property("M_Settings", settings)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    readback = grenade_component(unreal.load_asset(path)).get_editor_property("M_Settings")
    print(path.split("/")[-1], comp.get_name(), "saved=%s" % saved, readback.get_editor_property("GrenadeAbility"),
          readback.get_editor_property("GrenadeMesh").get_name(), readback.get_editor_property("Damage"),
          readback.get_editor_property("AoeRange"))
