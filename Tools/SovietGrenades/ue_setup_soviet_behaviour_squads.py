"""Gives Soviet squads and mortar teams their behaviour abilities: an ApplyBehaviourAbilityComponent per squad with one of
the behaviours from ue_create_soviet_behaviours.py. RusCloseAssault / RusMortarBarrage need an editor built with those
EBehaviourAbilityType values; set ONLY = ["AP"] before exec to run only the parts an older build knows.
Run through the UECP bridge's execute_python. Docs: Source/Docs/Abilities/SovietGrenades.md"""
import unreal

SQUADS = "/Game/RTS_Survival/Blueprints/Squads/"
BEH = "/Game/RTS_Survival/Blueprints/Behaviours/Weapon/Soviet/"
TW = SQUADS + "TeamWeapons/Soviet/"
ABILITIES = {
    "AP": dict(type="ARMOR_PIERCING_BULLETS", behaviour=BEH + "BP_Beh_Squad_APBullets_Soviet", cooldown=40, slot=6,
               name="APBulletsAbility"),
    "CloseAssault": dict(type="RUS_CLOSE_ASSAULT", behaviour=BEH + "BP_Beh_Squad_CloseAssault_Soviet", cooldown=35,
                         slot=6, name="CloseAssaultAbility"),
    "MortarBarrage": dict(type="RUS_MORTAR_BARRAGE", behaviour=BEH + "BP_Beh_MortarBarrage_Soviet", cooldown=60, slot=7,
                          name="RapidBarrageAbility"),
}
# Child Blueprints (Rus_Squad_PTRS_Soviet, the *_Soviet mortars) inherit the component from these.
ASSIGNMENTS = {
    SQUADS + "Rus/Rus_Squad_PTRS": "AP",
    SQUADS + "Rus/Rus_Squad_LargePTRSSquad": "AP",
    SQUADS + "Rus/Rus_Squad_MosinPTRS": "AP",
    SQUADS + "Rus/T2/Rus_T2Squad_RedHammer": "CloseAssault",
    SQUADS + "Rus/T2/Rus_T2Squad_RedHammerPTRS": "CloseAssault",
    SQUADS + "Rus/Rus_Squad_LargeMixBasic": "CloseAssault",
    TW + "80MMMortar/Rus_SquadTW_80mmMortar": "MortarBarrage",
    TW + "120Mortar/Rus_SquadTW_120mmMortar": "MortarBarrage",
}
ONLY = globals().get("ONLY")

sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary


def handles(bp):
    return sub.k2_gather_subobject_data_for_blueprint(bp)


def behaviour_components(bp):
    out = []
    for handle in handles(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj and obj.get_class().get_name() == "ApplyBehaviourAbilityComponent":
            out.append(obj)
    return out


def find_or_add(bp, ability_type, name):
    for comp in behaviour_components(bp):
        if comp.get_editor_property("BehaviourAbilitySettings").get_editor_property("BehaviourAbility") == ability_type:
            return comp
    params = unreal.AddNewSubobjectParams(parent_handle=handles(bp)[0],
                                          new_class=unreal.ApplyBehaviourAbilityComponent, blueprint_context=bp)
    handle, fail = sub.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError("add ApplyBehaviourAbilityComponent to %s failed: %s" % (bp.get_name(), fail))
    sub.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


for path, key in ASSIGNMENTS.items():
    if ONLY and key not in ONLY:
        continue
    spec = ABILITIES[key]
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        print("MISSING", path)
        continue
    ability_type = getattr(unreal.BehaviourAbilityType, spec["type"])
    bp = unreal.load_asset(path)
    comp = find_or_add(bp, ability_type, spec["name"])
    settings = comp.get_editor_property("BehaviourAbilitySettings")
    settings.set_editor_property("BehaviourAbility", ability_type)
    settings.set_editor_property("PreferredAbilityIndex", spec["slot"])
    settings.set_editor_property("Cooldown", spec["cooldown"])
    settings.set_editor_property("BehaviourApplied", unreal.load_asset(spec["behaviour"]).generated_class())
    comp.modify()
    comp.set_editor_property("BehaviourAbilitySettings", settings)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    readback = [c.get_editor_property("BehaviourAbilitySettings") for c in behaviour_components(unreal.load_asset(path))]
    print(path.split("/")[-1], comp.get_name(), "saved=%s" % saved,
          [(r.get_editor_property("BehaviourAbility"), r.get_editor_property("PreferredAbilityIndex"),
            r.get_editor_property("Cooldown"), r.get_editor_property("BehaviourApplied").get_name()) for r in readback])
