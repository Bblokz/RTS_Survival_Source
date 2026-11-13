"""Imports the Berlin ruin footprints ("A Schuttschuerze" rubble skirts + collapse heaps) for Ger_Berlin.

Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<wrapper that imports this module and calls run()>
or run() in an editor.

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/Footprints/FBX
  (generate_footprints.py; cm data, +X street front, pivot on the ground, ONE slot M_BerlinRuin_Rubble, colour attribute
  R soot / G plaster-brick / B dust exported linear, no UCX). Placement per kit piece: footprint_table.json (Unreal axes).
Destination: /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Footprints  SM_BerlinFoot_*
Legacy FBX importer, vertex colours replaced from the FBX, NO collision generated (no simple collision; the default
collision profile is set to NoCollision where the API allows; units walk and drive over the skirts), light map index 1,
MI_BerlinRuin_Rubble assigned by slot name. Only assets inside the Footprints folder are created or saved.
"""
import json

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/Footprints/FBX/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/'
HOME = ROOT + 'Footprints'
RUBBLE = ROOT + 'Materials/MI_BerlinRuin_Rubble'
SLOT = 'M_BerlinRuin_Rubble'
MESHES = tuple('SM_BerlinFoot_' + n for n in (
    'HalfHouse_A', 'HalfHouse_B', 'Shell_A', 'Shell_B', 'StubHeap', 'SlabPlot_A', 'SlabPlot_B', 'CornerL',
    'CornerTenement', 'Courtyard', 'RowStrip', 'CollapseHeap_S', 'CollapseHeap_L'))


def import_mesh(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)      # walkable: no collision at all
    data.set_editor_property('generate_lightmap_u_vs', False)       # UV1 is a packed unwrap from Blender
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def no_collision(mesh):
    """Drop any simple collision and make the default profile NoCollision. Returns what was done."""
    done = []
    body = mesh.get_editor_property('body_setup')
    if body is None:
        return ['no body setup']
    try:
        sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        sms.remove_collisions(mesh)
        done.append('remove_collisions')
    except Exception as exc:  # noqa: BLE001
        done.append('remove_collisions failed: %s' % exc)
    try:
        resp = body.get_editor_property('default_instance')
        resp.set_editor_property('collision_profile_name', 'NoCollision')
        resp.set_editor_property('collision_enabled', unreal.CollisionEnabled.NO_COLLISION)
        body.set_editor_property('default_instance', resp)
        done.append('profile NoCollision')
    except Exception as exc:  # noqa: BLE001
        done.append('profile failed: %s' % exc)
    try:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        done.append('simple-as-complex (no simple shapes -> no complex traces)')
    except Exception as exc:  # noqa: BLE001
        done.append('trace flag failed: %s' % exc)
    return done


def finish_mesh(name):
    mesh = unreal.load_asset(HOME + '/' + name)
    if mesh is None:
        raise RuntimeError('import made nothing for ' + name)
    slots = mesh.get_editor_property('static_materials')
    slot_names = [str(s.get_editor_property('material_slot_name')) for s in slots]
    if slot_names != [SLOT]:
        raise RuntimeError(f'{name}: slots {slot_names}, expected [{SLOT}]')
    inst = unreal.load_asset(RUBBLE) if unreal.EditorAssetLibrary.does_asset_exist(RUBBLE) else None
    if inst is not None:
        mesh.set_material(0, inst)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    col = no_collision(mesh)
    mesh.modify()
    saved = unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    agg = body.get_editor_property('agg_geom') if body else None
    shapes = sum(len(agg.get_editor_property(k)) for k in ('convex_elems', 'box_elems', 'sphere_elems',
                                                           'sphyl_elems')) if agg else 0
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    try:
        has_vc = bool(sms.has_vertex_colors(mesh))
    except Exception:  # noqa: BLE001
        has_vc = None
    profile = None
    if body:
        try:
            profile = str(body.get_editor_property('default_instance').get_editor_property('collision_profile_name'))
        except Exception:  # noqa: BLE001
            profile = None
    info = {'mesh': mesh.get_path_name(), 'saved': bool(saved), 'simple_collision_shapes': shapes,
            'collision_profile': profile, 'collision_steps': col,
            'bounds': [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
            'verts': sms.get_number_verts(mesh, 0), 'slots': slot_names,
            'material': mesh.get_material(0).get_name() if mesh.get_material(0) else None,
            'vertex_colors': has_vc, 'uv_channels': sms.get_num_uv_channels(mesh, 0),
            'light_map_index': mesh.get_editor_property('light_map_coordinate_index')}
    if shapes:
        raise RuntimeError(f'{name}: footprints must have no collision, got {shapes} shapes')
    unreal.log_warning('BERLIN_FOOTPRINTS ' + json.dumps(info))
    return info


def run():
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in MESHES:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    return [finish_mesh(name) for name in MESHES]


if __name__ == '__main__':
    run()
