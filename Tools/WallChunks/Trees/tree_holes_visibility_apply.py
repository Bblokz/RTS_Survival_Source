"""Makes dead-tree shell holes read on dark bark (2026-11-11, wall-holes Shorts: 12 holes on a trunk showed as 3-4 dark
dots). Run in a throwaway full editor (never the shared one), e.g.
  UnrealEditor-Win64-DebugGame-Cmd.exe <project> -ExecutePythonScript=<this file> -unattended -nosplash -nullrhi
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall

- MI_Env_DeadTree_1_WallHoles: the inside of a hole (back faces) is fresh pale wood instead of charred wood, so a hole is
  a bright scoop on the bark. (The rim scorch is a constant in MM_RTS_Env_WallHoles, shared with the large tent: left.)
- BP_Scorch_Dest_DeadTree WallHoleDamage: bProjectHitsOntoWallMesh (shells reported hits within 20 cm of the trunk's
  axis, inside the 55 cm trunk, so no hole reached the bark), HoleRadiusPerCalibreMm 0.4 -> 0.6, MaxHoleRadius 40 -> 50 cm (kept under a
  trunk's width, so a hole does not sever the trunk while the tree stands).
Writes Saved/WallChunks/Trees/visibility_apply.json.
"""
import json

import unreal

OUT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + 'WallChunks/Trees/visibility_apply.json'
MI = '/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/Materials/Holes/MI_Env_DeadTree_1_WallHoles'
BP = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/BP_Scorch_Dest_DeadTree'
PALE_WOOD = unreal.LinearColor(0.50, 0.34, 0.19, 1.0)
RADIUS_PER_MM = 0.6
MAX_RADIUS = 50.0
log = []
try:
    instance = unreal.load_asset(MI)
    if instance is None:
        found = unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_package_name(unreal.Name(MI))
        log.append('MI missing at %s (%d registry hits)' % (MI, len(found)))
    else:
        MEL = unreal.MaterialEditingLibrary
        before = (MEL.get_material_instance_vector_parameter_value(instance, 'WallHoleInnerColor'),
                  MEL.get_material_instance_scalar_parameter_value(instance, 'WallHoleInnerDarken'))
        MEL.set_material_instance_vector_parameter_value(instance, 'WallHoleInnerColor', PALE_WOOD)
        MEL.set_material_instance_scalar_parameter_value(instance, 'WallHoleInnerDarken', 1.0)
        MEL.update_material_instance(instance)
        log.append('MI inner before %s -> %s / darken 1.0' % (before, PALE_WOOD))
        log.append('MI saved %s' % unreal.EditorAssetLibrary.save_loaded_asset(instance, False))
    blueprint = unreal.load_asset(BP)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    done = False
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        component = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
        if isinstance(component, unreal.WallHoleDamageComponent):
            settings = component.get_editor_property('hole_settings')
            before = (settings.get_editor_property('hole_radius_per_calibre_mm'), settings.get_editor_property('max_hole_radius'))
            settings.set_editor_property('hole_radius_per_calibre_mm', RADIUS_PER_MM)
            settings.set_editor_property('max_hole_radius', MAX_RADIUS)
            component.set_editor_property('hole_settings', settings)
            grid = component.get_editor_property('grid_settings')
            grid.set_editor_property('project_hits_onto_wall_mesh', True)
            component.set_editor_property('grid_settings', grid)
            log.append('BP project hits onto mesh %s' % component.get_editor_property('grid_settings').get_editor_property(
                'project_hits_onto_wall_mesh'))
            after = component.get_editor_property('hole_settings')
            log.append('BP %s radius %s -> (%s, %s)' % (component.get_name(), before,
                                                        after.get_editor_property('hole_radius_per_calibre_mm'),
                                                        after.get_editor_property('max_hole_radius')))
            done = True
    if done:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        log.append('BP saved %s' % unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False))
    else:
        log.append('no WallHoleDamage component template found on the BP')
except Exception as error:  # noqa: BLE001
    log.append('ERROR %r' % error)
open(OUT, 'w').write(json.dumps(log, indent=1, default=str))
unreal.SystemLibrary.quit_editor()
