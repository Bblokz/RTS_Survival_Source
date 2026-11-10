"""Unreal side of the bunker chunk pipeline (option D on buildings). Use inside the editor:
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/bunker_chunks_ue.py', encoding='utf-8').read())

Steps (Source/Tools/WallChunks/BUNKER_AGENT_BRIEF.md has the whole recipe):
 1. export_source(mesh_path, out_dir)                      -> <out_dir>/<Mesh>_Source.fbx (all LODs + UCX)
 2. Blender (from a shell, not from here):
      D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P .../fracture_concrete_chunks.py -- \
          <Mesh>_Source.fbx <out_dir> <Mesh>_Chunks <concrete material names> chunks=<n> seed=<s>
 3. import_chunked(mesh_path, out_dir, target_folder)      -> <Mesh>_Chunks: a duplicate of the source asset
      (sockets, collision, settings kept) with the chunked LOD0 reimported, LODs removed (the chunk mask lives on
      LOD0 only), Nanite off, every original slot keeping its material; plus <Mesh>_Chunks_Debris_<n>.
 4. make_chunk_instance(slot_material, folder, uv_channel, name, broken=False/True, two_sided=...)
      masked instance for a concrete slot (and a darker, rougher one for the ConcreteBroken cap slot).
 5. build_layout(chunk_mesh_path, chunk_slots, debris_paths, damaged_by_slot, uv_channel)
      runs RTS.WallChunks.BuildLayoutFromMesh: chunk centres/areas from the mesh, debris piece per chunk.
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROP = unreal.MaterialProperty
CHUNK_SUFFIX = '_WallChunks'


def export_source(mesh_path, out_dir):
    """FBX of the source mesh. The folder must exist (create it from a shell first)."""
    mesh = unreal.load_asset(mesh_path)
    path = f'{out_dir}/{mesh.get_name()}_Source.fbx'
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = path
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.StaticMeshExporterFBX()
    if not unreal.Exporter.run_asset_export_task(task):
        raise RuntimeError('export failed: ' + path)
    return path


def _import_task(fbx, folder, name):
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('build_nanite', False)
    task.options = options
    return task


def import_chunked(mesh_path, out_dir, target_folder, debris_count=8):
    """Duplicate + reimport. Returns (chunk mesh, [debris meshes])."""
    source = unreal.load_asset(mesh_path)
    name = source.get_name() + '_Chunks'
    target = f'{target_folder}/{name}'
    if EAL.does_asset_exist(target):
        # A reimport merges slots by name with the old asset's and scrambles them: always import fresh. Refuse when
        # something already uses the old one (repoint it or ask first).
        referencers = [r for r in EAL.find_package_referencers_for_asset(target) if not r.startswith(target)]
        if referencers:
            raise RuntimeError(f'{target} is used by {referencers}; reimporting would scramble its slots')
        EAL.delete_asset(target)
    tasks = [_import_task(f'{out_dir}/{name}.fbx', target_folder, name)]
    tasks += [_import_task(f'{out_dir}/{name}_Debris_{i}.fbx', target_folder, f'{name}_Debris_{i}') for i in range(debris_count)]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    chunked = unreal.load_asset(target)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_lods(chunked)
    nanite = chunked.get_editor_property('nanite_settings')
    nanite.enabled = False
    chunked.set_editor_property('nanite_settings', nanite)
    # A fresh import: carry over what the source asset set up (the UCX hulls came through the FBX).
    for name_of_setting in ('light_map_coordinate_index', 'light_map_resolution'):
        chunked.set_editor_property(name_of_setting, source.get_editor_property(name_of_setting))
    source_body = source.get_editor_property('body_setup')
    chunked_body = chunked.get_editor_property('body_setup')
    if source_body and chunked_body:
        # The automated FBX import skips UCX hulls; copy the source's simple collision and trace flag directly.
        chunked_body.set_editor_property('agg_geom', source_body.get_editor_property('agg_geom'))
        chunked_body.set_editor_property('collision_trace_flag', source_body.get_editor_property('collision_trace_flag'))
    for socket in source.get_sockets_by_tag(''):
        copy = unreal.StaticMeshSocket(chunked)
        for prop in ('socket_name', 'relative_location', 'relative_rotation', 'relative_scale', 'tag'):
            copy.set_editor_property(prop, socket.get_editor_property(prop))
        chunked.add_socket(copy)
    # The FBX names each slot after its material: give every slot back the source material of that name, and check
    # the order matches the source's (Blueprint material overrides go by slot index). The cap slot is the caller's.
    source_materials = [slot.material_interface for slot in source.static_materials]
    by_name = {material.get_name(): material for material in source_materials if material}
    for index, slot in enumerate(chunked.static_materials):
        material = by_name.get(str(slot.material_slot_name))
        if material:
            chunked.set_material(index, material)
    order = [str(slot.material_slot_name) for slot in chunked.static_materials]
    expected = [material.get_name() if material else None for material in source_materials]
    if order[:len(expected)] != expected:
        # Slots used only by LOD1+ drop out (the chunk mesh is LOD0 only), shifting later indices.
        unreal.log_warning(f'CHUNK_IMPORT slots {order} vs source {expected}: fix Blueprint material overrides by index')
    sub_collision = len(chunked.get_editor_property('body_setup').get_editor_property('agg_geom').get_editor_property('convex_elems'))
    print('CHUNK_IMPORT collision hulls', sub_collision, 'sockets', [str(s.socket_name) for s in chunked.get_sockets_by_tag('')])
    EAL.save_loaded_asset(chunked, False)
    debris = []
    for index in range(debris_count):
        piece = unreal.load_asset(f'{target_folder}/{name}_Debris_{index}')
        if piece:
            subsystem.remove_collisions(piece)
            debris.append(piece)
    box_s, box_c = source.get_bounding_box(), chunked.get_bounding_box()
    print('CHUNK_IMPORT', name, 'tris', source.get_num_triangles(0), '->', chunked.get_num_triangles(0),
          'uv', subsystem.get_num_uv_channels(chunked, 0), 'lightmap uv', chunked.get_editor_property('light_map_coordinate_index'),
          'size', box_s.max - box_s.min, '->', box_c.max - box_c.min,
          'slots', [(str(s.material_slot_name), s.material_interface.get_name() if s.material_interface else None)
                    for s in chunked.static_materials])
    return chunked, debris


def _chunk_code():
    return '''// Chunk index (exact integer in the chunk UV channel's U) -> its bit in one of five 20-bit words (CPD 8..12).
float Index = floor(ChunkUv + 0.5);
float Word = floor(Index / 20.0);
float Bit = Index - Word * 20.0;
float Mask = Word < 0.5 ? M0 : (Word < 1.5 ? M1 : (Word < 2.5 ? M2 : (Word < 3.5 ? M3 : M4)));
float Hidden = Index > 99.5 ? 0.0 : fmod(floor(Mask / exp2(Bit)), 2.0);
return 1.0 - Hidden;'''


def chunk_master_for(source_master_path, uv_channel):
    """`<Master>_WallChunks_UV<n>`: masked copy of an opaque master that clips hidden chunks; built once, shared."""
    source = unreal.load_asset(source_master_path)
    target = f"{source.get_path_name().split('.')[0]}{CHUNK_SUFFIX}_UV{uv_channel}"
    if EAL.does_asset_exist(target):
        return unreal.load_asset(target)
    material = EAL.duplicate_asset(source.get_path_name().split('.')[0], target)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('opacity_mask_clip_value', 0.5)
    material.set_editor_property('used_with_niagara_mesh_particles', True)
    base = MEL.get_material_property_input_node(material, PROP.MP_BASE_COLOR)
    rough = MEL.get_material_property_input_node(material, PROP.MP_ROUGHNESS)
    if base is None:
        raise RuntimeError(target + ': BaseColor not connected (material attributes?)')
    tint = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, 200, -500)
    tint.set_editor_property('parameter_name', 'WallChunkTint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    tinted = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, 450, -400)
    MEL.connect_material_expressions(base, '', tinted, 'A')
    MEL.connect_material_expressions(tint, '', tinted, 'B')
    MEL.connect_material_property(tinted, '', PROP.MP_BASE_COLOR)
    if rough is not None:
        add = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, 200, -150)
        add.set_editor_property('parameter_name', 'WallChunkRoughnessAdd')
        add.set_editor_property('default_value', 0.0)
        summed = MEL.create_material_expression(material, unreal.MaterialExpressionAdd, 450, -150)
        MEL.connect_material_expressions(rough, '', summed, 'A')
        MEL.connect_material_expressions(add, '', summed, 'B')
        MEL.connect_material_property(summed, '', PROP.MP_ROUGHNESS)
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -300, 700)
    uv.set_editor_property('coordinate_index', uv_channel)
    u = MEL.create_material_expression(material, unreal.MaterialExpressionComponentMask, -100, 700)
    for channel in 'rgba':
        u.set_editor_property(channel, channel == 'r')
    MEL.connect_material_expressions(uv, '', u, '')
    custom = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, 200, 700)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom.set_editor_property('description', 'WallChunkVisible')
    custom.set_editor_property('code', _chunk_code())
    inputs = []
    for input_name in ['ChunkUv', 'M0', 'M1', 'M2', 'M3', 'M4']:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property('input_name', input_name)
        inputs.append(custom_input)
    custom.set_editor_property('inputs', inputs)
    MEL.connect_material_expressions(u, '', custom, 'ChunkUv')
    for index in range(5):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -100, 850 + index * 90)
        node.set_editor_property('parameter_name', f'WallChunkHiddenMask{index}')
        node.set_editor_property('use_custom_primitive_data', True)
        node.set_editor_property('primitive_data_index', 8 + index)
        MEL.connect_material_expressions(node, '', custom, f'M{index}')
    MEL.connect_material_property(custom, '', PROP.MP_OPACITY_MASK)
    unreal.SystemLibrary.execute_console_command(None, 'RTS.WallChunks.FinishMaskedMaster Material=' + target)
    MEL.recompile_material(material)
    EAL.save_loaded_asset(material, False)
    return material


def check_mask_live(material):
    """(masked PS, opaque PS): the chunk mask adds ~15; equal counts mean the mask compiled away."""
    source = unreal.load_asset(material.get_path_name().split('.')[0].rsplit(CHUNK_SUFFIX, 1)[0])
    return (MEL.get_statistics(material).num_pixel_shader_instructions,
            MEL.get_statistics(source).num_pixel_shader_instructions)


def make_chunk_instance(slot_material_path, folder, uv_channel, name, broken=False, two_sided=False):
    """Masked instance for a concrete slot. broken=True: darker, rougher, for the ConcreteBroken cap slot."""
    slot = unreal.load_asset(slot_material_path)
    target = f'{folder}/{name}'
    if isinstance(slot, unreal.MaterialInstance):
        parent = slot.get_editor_property('parent')
        if isinstance(parent, unreal.MaterialInstance):
            # Keep the parent's overrides: hang under the parent's own chunk instance.
            master = make_chunk_instance(parent.get_path_name().split('.')[0], folder, uv_channel, name + '_Parent',
                                         False, two_sided)
        else:
            master = chunk_master_for(slot.get_base_material().get_path_name(), uv_channel)
        instance = unreal.load_asset(target) if EAL.does_asset_exist(target) else \
            EAL.duplicate_asset(slot.get_path_name().split('.')[0], target)
    else:
        master = chunk_master_for(slot.get_path_name(), uv_channel)
        instance = unreal.load_asset(target) if EAL.does_asset_exist(target) else \
            unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(instance, master)
    if broken:
        MEL.set_material_instance_vector_parameter_value(instance, 'WallChunkTint', unreal.LinearColor(0.6, 0.58, 0.55, 1))
        MEL.set_material_instance_scalar_parameter_value(instance, 'WallChunkRoughnessAdd', 0.3)
    if two_sided:
        overrides = instance.get_editor_property('base_property_overrides')
        overrides.set_editor_property('override_two_sided', True)
        overrides.set_editor_property('two_sided', True)
        instance.set_editor_property('base_property_overrides', overrides)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance, False)
    return instance


def set_debris_material(debris, material):
    for piece in debris:
        for index in range(len(piece.static_materials)):
            piece.set_material(index, material)
        EAL.save_loaded_asset(piece, False)


def build_layout(chunk_mesh_path, chunk_slots, debris_paths, damaged_by_slot, uv_channel):
    """damaged_by_slot: one material path per mesh slot ('' keeps that slot's material)."""
    command = (f"RTS.WallChunks.BuildLayoutFromMesh Mesh={chunk_mesh_path} Slots={'|'.join(str(s) for s in chunk_slots)} "
               f"Debris={'|'.join(debris_paths)} Damaged={'|'.join(p or 'None' for p in damaged_by_slot)} "
               f"UvChannel={uv_channel}")
    unreal.SystemLibrary.execute_console_command(None, command)
    print('CHUNK_LAYOUT ran; check the log line RTS_WALL_CHUNK_AUTHORING layout RESULT')
