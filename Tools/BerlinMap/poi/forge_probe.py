"""Read-only probe for the Forge station build (run inside forge_editor_server.py via forge_cmd.py).

Prints: MI_Crystal1's parent and parameters; the components of the candidate Soviet building Blueprints used as the
base for the three counted targets; the Ger_Berlin station actor (POI_ForgeSurveyStation) and the mission manager.
"""
import unreal

CRYSTAL = "/Game/RTS_Survival/Blueprints/Resources/Crystals/Textures/MI_Crystal1"
CANDIDATES = [
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_Megaphone",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_RusBunker03_Soviet",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_CoolingTowers_Soviet",
]


def probe_crystal():
    mi = unreal.load_asset(CRYSTAL)
    print("CRYSTAL", mi.get_class().get_name(), "parent:", mi.get_editor_property("parent").get_path_name())
    for kind, getter in (("scalar", "scalar_parameter_values"), ("vector", "vector_parameter_values"),
                         ("texture", "texture_parameter_values")):
        for entry in mi.get_editor_property(getter):
            info = entry.get_editor_property("parameter_info")
            value = entry.get_editor_property("parameter_value")
            if kind == "texture" and value is not None:
                value = value.get_path_name()
            print("  ", kind, info.get_editor_property("name"), value)


def probe_blueprint(path):
    bp = unreal.load_asset(path)
    if bp is None:
        print("BP missing", path)
        return
    gen = bp.generated_class()
    cdo = unreal.get_default_object(gen)
    print("BP", path, "parent:", unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_name()
          if hasattr(unreal.BlueprintEditorLibrary, "get_blueprint_parent_class") else "?")
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None:
            continue
        extra = ""
        if isinstance(obj, unreal.StaticMeshComponent):
            mesh = obj.get_editor_property("static_mesh")
            extra = " mesh=" + (mesh.get_path_name() if mesh else "None")
        print("   comp", obj.get_name(), obj.get_class().get_name(), extra)
    mesh_comp = cdo.get_editor_property("building_mesh_component")
    if mesh_comp:
        mesh = mesh_comp.get_editor_property("static_mesh")
        print("   CDO BuildingMeshComponent mesh:", mesh.get_path_name() if mesh else None)


def probe_map():
    world = unreal.EditorLevelLibrary.get_editor_world()
    print("WORLD", world.get_name() if world else None)
    if world is None:
        return
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        label = actor.get_actor_label()
        cls = actor.get_class().get_name()
        if label.startswith("POI_Forge") or "MissionManager" in cls or label.startswith("FORGE_"):
            print("  ACTOR", label, cls, actor.get_actor_location(), actor.get_actor_rotation())


probe_crystal()
for candidate in CANDIDATES:
    try:
        probe_blueprint(candidate)
    except Exception as exc:  # keep probing the others
        print("BP probe failed", candidate, exc)
probe_map()
