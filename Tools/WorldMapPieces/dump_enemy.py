"""Lists the components of the eight enemy world map object Blueprints (headless). Log: Saved/WorldMapPieces/enemy.txt"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402

ENEMY = "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/"
BLUEPRINTS = ["BP_EnemyWorldObject", "HQ/BP_Enemy_HQ", "Wall/BP_EnemyWall", "Factory/BP_EnemyFactory",
              "Airfield/BP_EnemyAirfield", "Barracks/BP_EnemyBarracks", "SupplyDepot/BP_EnemySupplyDepot",
              "ResearchFacility/BP_EnemyResearchFacility", "FortifiedCheckPoint/BP_EnemyFortifiedCheckpoint"]
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for name in BLUEPRINTS:
    blueprint = unreal.load_asset(ENEMY + name)
    if blueprint is None:
        lib.log("=== %s MISSING" % name)
        continue
    generated = blueprint.generated_class()
    super_class = unreal.SystemLibrary.get_class_display_name(generated)
    lib.log("=== %s class=%s cdo=%s" % (name, super_class, unreal.get_default_object(generated).get_class().get_name()))
    try:
        lib.log("  parent: " + str(unreal.BlueprintEditorLibrary.get_blueprint_asset(blueprint).generated_class()))
    except Exception:
        pass
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        template = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(
            subsystem.k2_find_subobject_data_from_handle(handle))
        if template is None or template.get_name() in seen or not isinstance(template, unreal.SceneComponent):
            continue
        seen.add(template.get_name())
        line = "  %s (%s) loc=(%.0f %.0f %.0f) yaw=%.0f scale=(%.3f %.3f %.3f)" % (
            template.get_name(), template.get_class().get_name(), template.relative_location.x,
            template.relative_location.y, template.relative_location.z, template.relative_rotation.yaw,
            template.relative_scale3d.x, template.relative_scale3d.y, template.relative_scale3d.z)
        if isinstance(template, unreal.StaticMeshComponent) and template.static_mesh:
            mesh = template.static_mesh
            bounds = mesh.get_bounds()
            line += " mesh=%s tris=%d extent=(%.0f %.0f %.0f) origin=(%.0f %.0f %.0f) mats=%s collision=%s" % (
                mesh.get_name(), mesh.get_num_triangles(0), bounds.box_extent.x, bounds.box_extent.y,
                bounds.box_extent.z, bounds.origin.x, bounds.origin.y, bounds.origin.z,
                [m.get_name() if m else None for m in template.get_materials()],
                template.get_collision_profile_name())
        if isinstance(template, unreal.ChildActorComponent):
            line += " child=%s" % (template.child_actor_class.get_name() if template.child_actor_class else None)
        lib.log(line)
    default_object = unreal.get_default_object(generated)
    for field in ("m_map_item_ui_data",):
        try:
            lib.log("  ui: " + default_object.get_editor_property(field).export_text()[:300])
        except Exception as error:
            lib.log("  no %s: %s" % (field, error))
lib.write_log("enemy.txt")
