"""Gives Ger_BXP_GerPit_Mortar its own mesh as the building mesh component's default.

The Blueprint was made from the Flak 88 pit and kept SM_GerFlak88Pit on the component template. A pit built through
the HQ swaps to SM_GerMortarPit (the mesh its graph passes to InitBuildingExpansion), but one placed in a level keeps
the template mesh: it looked like a Flak 88 pit and offered the Flak 88 pit's trench anchors (Rear, Right) instead of
its own TrenchPort_RearRight, whose neck is listed on its TrenchAnchors component.

Run in a throwaway editor (needs a DLL with UFieldTrenchAnchorComponent so the Blueprint keeps its anchors):
    UnrealEditor-Cmd.exe <project>.uproject -nullrhi -unattended -ExecCmds="py <this file>"
Result: Saved/TrenchAnchorAudit/fix_mortar_pit.txt
"""
import os
import unreal

BLUEPRINT = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Mortar"
WRONG_MESH = "SM_GerFlak88Pit"
RIGHT_MESH = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/SM_GerMortarPit"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "TrenchAnchorAudit", "fix_mortar_pit.txt")
lines = []


def log(text):
    lines.append(text)
    unreal.log("[FixMortarPit] " + text)


def templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        template = library.get_object_for_blueprint(library.get_data(handle), blueprint)
        if template is not None:
            yield template


def run():
    blueprint = unreal.load_asset(BLUEPRINT)
    right_mesh = unreal.load_asset(RIGHT_MESH)
    if blueprint is None or right_mesh is None:
        log("FAIL could not load the Blueprint or " + RIGHT_MESH)
        return
    changed = 0
    for template in templates(blueprint):
        if not isinstance(template, unreal.StaticMeshComponent):
            continue
        mesh = template.get_editor_property("static_mesh")
        if mesh is not None and mesh.get_name() == WRONG_MESH:
            template.modify()
            template.set_editor_property("static_mesh", right_mesh)
            log("set %s: %s -> %s" % (template.get_name(), WRONG_MESH, right_mesh.get_name()))
            changed += 1
    if changed == 0:
        log("nothing to change (no component template with %s)" % WRONG_MESH)
        return
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    log("compiled, saved=%s" % saved)
    # Read back: the template carries the mortar mesh, and the anchors survived the save.
    for template in templates(unreal.load_asset(BLUEPRINT)):
        if isinstance(template, unreal.StaticMeshComponent) and template.get_editor_property("static_mesh"):
            log("after: %s mesh=%s" % (template.get_name(), template.get_editor_property("static_mesh").get_name()))
        if isinstance(template, unreal.FieldTrenchAnchorComponent):
            sockets = [str(d.get_editor_property("socket_name")) for d in template.get_editor_property("anchors")]
            log("after: anchors " + ", ".join(sockets))


try:
    run()
finally:
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    unreal.SystemLibrary.quit_editor()
