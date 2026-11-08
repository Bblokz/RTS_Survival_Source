"""Bakes the Jaguar's 5-plate Schuerzen set (plates, bolts, hanger brackets) onto the E-25 and E-25 Hunter hulls.

Writes <hull>_Skirts next to each source hull (a duplicate, so collision and settings carry over), then builds
<hull>_Skirts_AddOnPanels with the Panther D debris plate. The E-25 Assault keeps its own scrap skirts
(SM_Hull_E25_Assault in build_panel_hulls.py).
Run headless (no editor may have the targets open):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -nullrhi -nosplash -nosound
    -DisablePlugins=Fab,BpGeneratorUltimate
DRYRUN=1 only reports triangle counts.
"""
import os
import unreal

R = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle'
JAGUAR = R + '/Ger_Medium/Jaguar/Meshes/SM_Ger_Jaguar_Hull'
DEBRIS = R + '/Ger_Heavy/PantherD/Meshes/SM_PantherD_AddOnPanels_Debris'
TARGETS = [R + '/Ger_Heavy/E25/HullAndTurret/SM_E25_Hull', R + '/Ger_Heavy/E25/Hunter/SM_E25_Hunter_Hull']
# Jaguar plate set spans X -240.6..244.8, |Y| 140.4..165, Z 84..141.6. E-25 fender: |Y| 191-193, underside Z ~145,
# mudguards from X -238 and 232. Move the set out 45 cm, up 3.5 cm and shrink it 3% along X to sit between them.
SCALE_X = 0.9687
SHIFT_X = -5.03
SHIFT_Y = 45.0
SHIFT_Z = 3.5
DRY = os.environ.get('DRYRUN') == '1'
GS = unreal.GeometryScript_AssetUtils


def read(mesh):
    result, outcome = GS.copy_mesh_from_static_mesh_v2(
        mesh, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL),
        use_section_materials=False)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError(f'read failed {mesh.get_path_name()}')
    return result


def skirt_side(sign):
    mesh = read(unreal.load_asset(JAGUAR))
    low, high = (-170.0, -138.0) if sign < 0 else (138.0, 170.0)
    box = unreal.Box(unreal.Vector(-250, low, 80), unreal.Vector(255, high, 146))
    _, keep = unreal.GeometryScript_MeshSelection.select_mesh_elements_in_box(mesh, box)
    _, drop = unreal.GeometryScript_MeshSelection.invert_mesh_selection(mesh, keep)
    unreal.GeometryScript_MeshEdits.delete_selected_triangles_from_mesh(mesh, drop)
    transform = unreal.Transform(unreal.Vector(SHIFT_X, sign * SHIFT_Y, SHIFT_Z), unreal.Rotator(0, 0, 0),
                                 unreal.Vector(SCALE_X, 1, 1))
    unreal.GeometryScript_MeshTransforms.transform_mesh(mesh, transform)
    unreal.log(f'PJBAKE side {sign} tris={mesh.get_triangle_count()}')
    return mesh


for source_path in TARGETS:
    source = unreal.load_asset(source_path)
    target_path = source_path + '_Skirts'
    if not DRY:
        target = (unreal.load_asset(target_path) if unreal.EditorAssetLibrary.does_asset_exist(target_path)
                  else unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path))
    combined = read(source)
    count = combined.get_triangle_count()
    for sign in (-1, 1):
        side = skirt_side(sign)
        count += side.get_triangle_count()
        unreal.GeometryScript_MeshEdits.append_mesh(combined, side, unreal.Transform())
    if combined.get_triangle_count() != count:
        raise RuntimeError('combined hull is missing geometry')
    if DRY:
        unreal.log(f'PJBAKE dry {source_path} tris={count}')
        continue
    _, outcome = GS.copy_mesh_to_static_mesh(combined, target, unreal.GeometryScriptCopyMeshToAssetOptions(),
                                             unreal.GeometryScriptMeshWriteLOD(), use_section_materials=False)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('write failed')
    for index, slot in enumerate(source.static_materials):
        target.set_material(index, slot.material_interface)
    if not unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False):
        raise RuntimeError('save failed')
    b = target.get_bounding_box()
    unreal.log(f'PJBAKE wrote {target_path} tris={count} slots={len(target.static_materials)} '
               f'collision={target.get_editor_property("complex_collision_mesh")} '
               f'src_collision={source.get_editor_property("complex_collision_mesh")} bounds={b.min} {b.max}')
    unreal.SystemLibrary.execute_console_command(
        None, f'RTS.AddOnArmor.BuildPanelHull Source={target_path} Target={target_path}_AddOnPanels MinAbsY=193.5 '
              f'MinZ=85 MaxZ=150 MinPanelArea=8000 MaxIslandLength=110 DebrisMesh={DEBRIS}')
unreal.log('PJBAKE done')
