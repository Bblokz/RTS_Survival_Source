"""Adds UFieldTrenchAnchorComponent to the German pit and bunker Blueprints and fills in each socket's neck and plug.

Needs an editor built with the component (it is new C++): run it headlessly on a DebugGame build, e.g.
    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
        -script="Source/Tools/GerPits/ue_gerpit_trench_anchor_components.py" -unattended -nullrhi
Running it again only rewrites the anchor lists. Socket contract: Source/Docs/Units/GerPitTrenchAnchors.md.
"""
import unreal

ROOT = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/"
MESHES = ROOT + "Meshes/"
TWIN_MESHES = ROOT + "TwinMG42Pit/Meshes/"
COMPONENT_NAME = "TrenchAnchors"

HOWITZER = [
    ("TrenchPort_Rear", MESHES + "SM_GerPit_Howitzer_TrenchNeck_Rear", None),
    ("TrenchPort_RearLeft", MESHES + "SM_GerPit_Howitzer_TrenchNeck_Side", MESHES + "SM_GerPit_Howitzer_TrenchPlug_RearLeft"),
    ("TrenchPort_RearRight", MESHES + "SM_GerPit_Howitzer_TrenchNeck_Side", MESHES + "SM_GerPit_Howitzer_TrenchPlug_RearRight"),
]

# Blueprint -> (socket, neck mesh, plug mesh or None)
PITS = {
    ROOT + "Ger_BXP_GerPit_Pak40": [("TrenchPort_Rear", MESHES + "SM_GerPit_Pak_TrenchNeck_Rear", None)],
    ROOT + "Ger_BXP_GerPit_Flak37": [("TrenchPort_Rear", MESHES + "SM_GerPit_Flak_TrenchNeck_Rear", None)],
    ROOT + "Ger_BXP_GerPit_LeFH18": HOWITZER,
    ROOT + "Ger_BXP_GerPit_SFH18": HOWITZER,
    ROOT + "Ger_BXP_GerPit_Mortar": [("TrenchPort_RearRight", MESHES + "SM_GerMortarPit_TrenchNeck_RearRight", None)],
    ROOT + "Ger_BXP_GerPit_Flak88": [
        ("TrenchPort_Rear", MESHES + "SM_GerFlak88Pit_TrenchNeck_Rear", None),
        ("TrenchPort_Right", MESHES + "SM_GerFlak88Pit_TrenchNeck_Right", MESHES + "SM_GerFlak88Pit_TrenchPlug_Right"),
    ],
    ROOT + "TwinMG42Pit/Ger_BXP_GerPit_TwinMG42": [
        ("TrenchPort_Left", TWIN_MESHES + "SM_GerTwinMG42Pit_TrenchNeck_Left", TWIN_MESHES + "SM_GerTwinMG42Pit_TrenchPlug_Left"),
        ("TrenchPort_Right", TWIN_MESHES + "SM_GerTwinMG42Pit_TrenchNeck_Right", TWIN_MESHES + "SM_GerTwinMG42Pit_TrenchPlug_Right"),
    ],
}

# The Breakthrough bunkers have no neck or plug meshes yet: their TrenchPort_ sockets (set by their import scripts,
# Tools/BreakthroughBuildings/ue_flak88_bunker_import.py and ue_flakvierling_bunker_import.py) are anchors as they are.
BUNKERS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/"
PITS[BUNKERS + "Flak88Bunker/Ger_BXP_Flak88Bunker"] = []
PITS[BUNKERS + "FlakvierlingBunker/Ger_BXP_FlakvierlingBunker"] = []


def object_path(package_path):
    name = package_path.rsplit("/", 1)[1]
    return package_path + "." + name


def find_or_add_component(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in handles:
        data = library.get_data(handle)
        template = library.get_object_for_blueprint(data, blueprint)
        if isinstance(template, unreal.FieldTrenchAnchorComponent):
            return template
    params = unreal.AddNewSubobjectParams(
        parent_handle=handles[0], new_class=unreal.FieldTrenchAnchorComponent, blueprint_context=blueprint)
    new_handle, fail_reason = subsystem.add_new_subobject(params)
    if not fail_reason.is_empty():
        raise RuntimeError("could not add the component: " + str(fail_reason))
    subsystem.rename_subobject(new_handle, unreal.Text(COMPONENT_NAME))
    return library.get_object_for_blueprint(library.get_data(new_handle), blueprint)


def make_definition(socket, neck, plug):
    for mesh in (neck, plug):
        if mesh and not unreal.EditorAssetLibrary.does_asset_exist(mesh):
            raise RuntimeError("missing mesh " + mesh)
    definition = unreal.FieldTrenchAnchorDefinition()
    text = '(SocketName="{}",NeckMesh="{}",PlugMesh={})'.format(
        socket, object_path(neck), '"' + object_path(plug) + '"' if plug else "None")
    definition.import_text(text)
    return definition


def run():
    for blueprint_path, anchors in PITS.items():
        blueprint = unreal.load_asset(blueprint_path)
        if not blueprint:
            unreal.log_error("[TrenchAnchors] missing " + blueprint_path)
            continue
        template = find_or_add_component(blueprint)
        template.set_editor_property("anchors", [make_definition(*anchor) for anchor in anchors])
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        saved = unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
        written = [d.export_text() for d in template.get_editor_property("anchors")]
        unreal.log("[TrenchAnchors] {} saved={} anchors={}".format(blueprint_path, saved, written))


run()
